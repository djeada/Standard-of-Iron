#include "route_follow_system.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/entity.h"
#include "movement_orders_assignment.h"
#include "route_follow_system_arrival.h"
#include "route_follow_system_progress.h"
#include "route_follow_system_stall.h"
#include "route_follow_system_steering.h"
#include "systems/formation_combat_geometry.h"
#include "systems/navigation/nav_grid.h"
#include "util/planar_math.h"

namespace Game::Systems {

namespace {

constexpr float k_resolved_goal_progress_epsilon_sq = 0.01F;
constexpr float k_clearance_repath_threshold = 0.25F;
constexpr std::uint64_t k_route_prune_interval_ticks = 600U;

void forget_stall_window(Engine::Core::MovementStallFacts& stall) {
  stall.window_valid = false;
  stall.window_seconds = 0.0F;
}

void rest_under_gate(FollowFrame& frame, MovementGate gate) {
  auto* facts = &frame.facts;
  if (gate == MovementGate::Dead) {
    facts->progress.state = Engine::Core::MovementOrderState::Idle;
  } else if (gate == MovementGate::DirectControl) {
    RouteFollowSystem::Steering::publish_direct_control_intent(
        frame.entity, frame.transform, *facts);
  }

  forget_stall_window(facts->progress.stall);
  facts->progress.stall.stalled_seconds = 0.0F;
  facts->progress.stall.no_closer_seconds = 0.0F;
  facts->progress.stall.rung = Engine::Core::MovementRecoveryRung::None;
}

} // namespace

auto RouteFollowSystem::remaining_route_length(
    const Engine::Core::MovementComponent& movement,
    float position_x,
    float position_z) -> float {
  if (!movement.get_has_target()) {
    return 0.0F;
  }
  if (!movement.has_waypoints()) {
    return Game::Systems::planar_length(movement.get_target_x() - position_x,
                                        movement.get_target_y() - position_z);
  }

  auto const& path = movement.get_path();
  std::size_t const index = movement.get_path_index();
  float total = Game::Systems::planar_length(path[index].first - position_x,
                                             path[index].second - position_z);
  for (std::size_t step = index + 1U; step < path.size(); ++step) {
    total += Game::Systems::planar_length(path[step].first - path[step - 1U].first,
                                          path[step].second - path[step - 1U].second);
  }
  return total;
}

void RouteFollowSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  world->each<Engine::Core::MovementComponent>(
      [this, world, delta_time](Engine::Core::EntityID id,
                                Engine::Core::MovementComponent&) {
        auto* entity = world->get_entity(id);
        if (entity != nullptr) {
          follow(*entity, *world, delta_time);
        }
      });

  if (world->tick_id() - m_prune_tick >= k_route_prune_interval_ticks) {
    m_prune_tick = world->tick_id();
    std::erase_if(m_routes, [world](auto const& entry) {
      return world->get_entity(entry.first) == nullptr;
    });
  }
}

void RouteFollowSystem::follow(Engine::Core::Entity& entity,
                               Engine::Core::World& world,
                               float delta_time) {
  Engine::Core::EntityID const id = entity.get_id();
  auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
  auto* movement = world.try_get<Engine::Core::MovementComponent>(id);
  auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  if (transform == nullptr || movement == nullptr || unit == nullptr) {
    return;
  }

  auto* facts =
      Engine::Core::get_or_add_component<Engine::Core::MovementFactsComponent>(&entity);
  if (facts == nullptr) {
    return;
  }
  facts->begin_tick();
  facts->previous_root.valid = true;
  facts->previous_root.x = transform->position.x;
  facts->previous_root.z = transform->position.z;
  facts->previous_root.yaw = transform->rotation.y;

  FollowFrame frame{world, entity, *transform, *movement, *unit, *facts, delta_time};

  MovementGate const gate = classify_movement_gate(entity);
  if (gate != MovementGate::RouteFollowing) {
    rest_under_gate(frame, gate);
    return;
  }
  follow_route(frame);
}

void RouteFollowSystem::follow_route(FollowFrame& frame) {
  auto& world = frame.world;
  auto& entity = frame.entity;
  auto* transform = &frame.transform;
  auto* movement = &frame.movement;
  auto* unit = &frame.unit;
  auto* facts = &frame.facts;
  float const delta_time = frame.delta_time;
  Engine::Core::EntityID const id = entity.get_id();

  refresh_clearance(frame);
  Steering::publish_route_facts(frame);

  auto const* stamina = world.try_get<Engine::Core::StaminaComponent>(id);
  float const max_speed = formation_navigation_speed(entity, *unit, stamina);

  if (ObjectiveStall::track(frame, max_speed)) {
    m_routes.erase(id);
    return;
  }

  QVector3D const current_pos(transform->position.x, 0.0F, transform->position.z);
  bool const current_position_allowed = is_movement_point_allowed(current_pos, entity);

  switch (check_goal(frame, current_position_allowed)) {
  case GoalCheck::Handled:
    return;
  case GoalCheck::Abandoned:
    m_routes.erase(id);
    return;
  case GoalCheck::Fine:
    break;
  }

  if (!movement->get_has_target()) {
    if (Engine::Core::is_active_movement_state(facts->progress.state)) {
      facts->progress.state = Engine::Core::MovementOrderState::Cancelled;
    }
    facts->progress.holding_at_obstruction = false;
    return;
  }

  auto& route = m_routes[id];
  bool const route_changed = sync_route(frame, route);

  float const arrive_radius = Steering::arrive_radius_for(frame, max_speed);
  auto const aim = Steering::aim_along_route(frame, route, max_speed);

  if (!RouteProgress::update_progress(
          *movement, *facts, aim.remaining, route_changed, delta_time)) {
    return;
  }
  if (RouteArrival::resolve(
          frame, route, aim, arrive_radius, current_position_allowed)) {
    return;
  }
  Steering::publish_desired_motion(frame, aim, arrive_radius, max_speed);
}

void RouteFollowSystem::refresh_clearance(FollowFrame& frame) {
  auto* movement = &frame.movement;
  auto* transform = &frame.transform;
  auto* facts = &frame.facts;
  auto& entity = frame.entity;
  float const previous_clearance = movement->get_navigation_clearance();
  movement->set_navigation_clearance(
      FormationCombat::formation_navigation_clearance(entity));
  if (movement->get_has_target() && movement->get_has_requested_goal() &&
      std::abs(previous_clearance - movement->get_navigation_clearance()) >
          k_clearance_repath_threshold) {
    MovementSystem::Assignment::assign_navigation_target(
        NavGrid::get_pathfinder(),
        *transform,
        *movement,
        QVector3D(
            movement->get_requested_goal_x(), 0.0F, movement->get_requested_goal_z()));
    ++facts->progress.repath_count;
    facts->progress.repath_reason =
        Engine::Core::MovementRepathReason::ClearanceChanged;
  }
}

auto RouteFollowSystem::check_goal(FollowFrame& frame,
                                   bool current_position_allowed) -> GoalCheck {
  auto* movement = &frame.movement;
  auto* facts = &frame.facts;
  auto& transform = frame.transform;
  QVector3D const current_pos(transform.position.x, 0.0F, transform.position.z);
  QVector3D const final_goal(movement->get_goal_x(), 0.0F, movement->get_goal_y());
  bool const destination_allowed = is_movement_point_allowed(final_goal, frame.entity);

  if (!current_position_allowed && !movement->get_escape_active() &&
      MovementSystem::Assignment::assign_local_recovery_move(
          current_pos, RouteProgress::order_goal_of(*movement), movement)) {
    facts->progress.state = Engine::Core::MovementOrderState::Recovering;
    facts->progress.repath_reason =
        Engine::Core::MovementRepathReason::RecoveryEscalation;
    return GoalCheck::Handled;
  }

  if (movement->get_has_target() && !destination_allowed && current_position_allowed) {
    Point const requested_goal = NavGrid::world_to_grid(final_goal.x(), final_goal.z());
    auto const nearest_goal = NavGrid::find_nearest_walkable_grid(requested_goal, 32);
    if (!nearest_goal.has_value()) {
      ObjectiveStall::abandon(
          frame.entity, *movement, *facts, RouteProgress::order_goal_of(*movement));
      return GoalCheck::Abandoned;
    }

    QVector3D const resolved_goal = NavGrid::grid_to_world(*nearest_goal);
    float const resolved_dx = resolved_goal.x() - final_goal.x();
    float const resolved_dz = resolved_goal.z() - final_goal.z();
    if (resolved_dx * resolved_dx + resolved_dz * resolved_dz >
        k_resolved_goal_progress_epsilon_sq) {
      MovementSystem::Assignment::retarget_unit(
          frame.world, frame.entity.get_id(), resolved_goal);
      ++facts->progress.repath_count;
      facts->progress.repath_reason = Engine::Core::MovementRepathReason::GoalChanged;
      facts->progress.state = Engine::Core::MovementOrderState::Repathing;
      return GoalCheck::Handled;
    }
  }
  return GoalCheck::Fine;
}

auto RouteFollowSystem::sync_route(FollowFrame& frame, MovementRoute& route) -> bool {
  auto* movement = &frame.movement;
  auto* transform = &frame.transform;
  auto* facts = &frame.facts;
  bool route_changed = false;
  bool const new_route_revision =
      route.route_revision() != movement->get_route_revision();
  if (!route.valid() || new_route_revision) {
    route_changed = true;
    if (new_route_revision) {

      facts->progress.holding_at_obstruction = false;
    }

    route.build(movement->get_route_revision(),
                movement->get_topology_revision(),
                transform->position.x,
                transform->position.z,
                movement->get_path(),
                movement->get_path_index(),
                movement->get_target_x(),
                movement->get_target_y());
  } else if (!movement->get_path().empty()) {

    auto const& last = movement->get_path().back();
    auto const [final_x, final_z] = route.final_point();
    if (Game::Systems::planar_length(last.first - final_x, last.second - final_z) >
        1.0e-4F) {
      route.update_final_point(last.first, last.second);
    }
  }
  return route_changed;
}

auto RouteFollowSystem::route_for(Engine::Core::EntityID entity_id) const
    -> const MovementRoute* {
  auto const found = m_routes.find(entity_id);
  return found == m_routes.end() ? nullptr : &found->second;
}

auto RouteFollowSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent,
            TransformComponent,
            AttackComponent,
            StaminaComponent,
            CommanderComponent,
            HoldModeComponent,
            BuilderProductionComponent,
            PendingRemovalComponent>{},
      Writes<MovementComponent, MovementFactsComponent, GuardModeComponent>{});
}

} // namespace Game::Systems