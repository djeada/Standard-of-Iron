#include "route_follow_system.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../core/entity.h"
#include "../formation/army_formation_registry.h"
#include "../units/spawn_type.h"
#include "combat_rules.h"
#include "command_service.h"
#include "defensive_unit_layout_service.h"
#include "formation_combat_geometry.h"
#include "movement_system.h"
#include "nav_grid.h"
#include "order_service.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {

constexpr int k_max_waypoint_skip_count = 4;
constexpr float k_resolved_goal_progress_epsilon_sq = 0.01F;
constexpr float k_clearance_repath_threshold = 0.25F;

} // namespace

auto is_movement_point_allowed(const QVector3D& pos,
                               const Engine::Core::Entity& entity) -> bool {
  if (auto const* builder_prod =
          entity.get_component<Engine::Core::BuilderProductionComponent>();
      builder_prod != nullptr && builder_prod->in_progress &&
      builder_prod->at_construction_site && builder_prod->has_task_target &&
      builder_prod->task_target_id != 0) {
    Point const position_grid = NavGrid::world_to_grid(pos.x(), pos.z());
    Point const target_grid = NavGrid::world_to_grid(builder_prod->task_target_x,
                                                     builder_prod->task_target_z);
    if (position_grid.x == target_grid.x && position_grid.y == target_grid.y) {
      return true;
    }
  }

  auto const* movement = entity.get_component<Engine::Core::MovementComponent>();
  auto* pathfinder = NavGrid::get_pathfinder();
  bool navigation_allows = NavGrid::is_world_position_walkable(pos);
  if (pathfinder != nullptr && movement != nullptr) {
    pathfinder->update_navigation_grid();
    navigation_allows = pathfinder->is_world_position_walkable(
        pos,
        movement->get_can_enter_forest() ? Pathfinding::Passability::Light
                                         : Pathfinding::Passability::Heavy,
        0.0F);
  }
  return navigation_allows;
}

auto max_navigation_speed(const Engine::Core::UnitComponent& unit,
                          const Engine::Core::StaminaComponent* stamina) -> float {
  float speed = std::max(0.1F, unit.speed);
  if (stamina != nullptr && stamina->is_running) {
    speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
  }
  return speed;
}

auto formation_navigation_speed(const Engine::Core::Entity& entity,
                                const Engine::Core::UnitComponent& unit,
                                const Engine::Core::StaminaComponent* stamina)
    -> float {
  return max_navigation_speed(unit, stamina) *
         DefensiveUnitLayoutService::move_speed_multiplier(entity) *
         Game::Formation::ArmyFormationRuntime::move_speed_multiplier(entity);
}

auto classify_movement_gate(const Engine::Core::Entity& entity) -> MovementGate {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0 ||
      entity.has_component<Engine::Core::PendingRemovalComponent>()) {
    return MovementGate::Dead;
  }

  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander != nullptr && (commander->jump_active || commander->fpv_controlled)) {
    return MovementGate::DirectControl;
  }

  auto const* hold_mode = entity.get_component<Engine::Core::HoldModeComponent>();
  if (hold_mode != nullptr && (hold_mode->active || hold_mode->exit_cooldown > 0.0F)) {
    return MovementGate::HoldMode;
  }

  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (attack != nullptr && attack->in_melee_lock &&
      CombatRules::participates_in_rts_melee_lock(&entity)) {
    return MovementGate::MeleeLock;
  }

  auto const* builder_prod =
      entity.get_component<Engine::Core::BuilderProductionComponent>();
  if (builder_prod != nullptr && builder_prod->bypass_movement_active) {
    return MovementGate::BuilderBypass;
  }

  return MovementGate::RouteFollowing;
}

auto RouteFollowSystem::remaining_route_length(
    const Engine::Core::MovementComponent& movement,
    float position_x,
    float position_z) -> float {
  if (!movement.get_has_target()) {
    return 0.0F;
  }
  if (!movement.has_waypoints()) {
    return std::hypot(movement.get_target_x() - position_x,
                      movement.get_target_y() - position_z);
  }

  auto const& path = movement.get_path();
  std::size_t const index = movement.get_path_index();
  float total =
      std::hypot(path[index].first - position_x, path[index].second - position_z);
  for (std::size_t step = index + 1U; step < path.size(); ++step) {
    total += std::hypot(path[step].first - path[step - 1U].first,
                        path[step].second - path[step - 1U].second);
  }
  return total;
}

void RouteFollowSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  world->each<Engine::Core::MovementComponent>(
      [world, delta_time](Engine::Core::EntityID id, Engine::Core::MovementComponent&) {
        auto* entity = world->get_entity(id);
        if (entity != nullptr) {
          follow(*entity, *world, delta_time);
        }
      });
}

void RouteFollowSystem::follow(Engine::Core::Entity& entity,
                               Engine::Core::World& world,
                               float delta_time) {
  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  auto* movement = entity.get_component<Engine::Core::MovementComponent>();
  auto* unit = entity.get_component<Engine::Core::UnitComponent>();
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

  MovementGate const gate = classify_movement_gate(entity);
  if (gate != MovementGate::RouteFollowing) {
    if (gate == MovementGate::Dead) {
      facts->progress.state = Engine::Core::MovementOrderState::Idle;
    }
    return;
  }

  // The formation envelope changes when a troop takes casualties or changes
  // layout. A materially different envelope invalidates the route it was
  // planned against, so this is a declared repath cause rather than a silent
  // reuse of a route the body no longer fits.
  float const previous_clearance = movement->get_navigation_clearance();
  movement->set_navigation_clearance(
      FormationCombat::formation_navigation_clearance(entity));
  if (movement->get_has_target() && movement->get_has_requested_goal() &&
      std::abs(previous_clearance - movement->get_navigation_clearance()) >
          k_clearance_repath_threshold) {
    MovementSystem::assign_navigation_target(
        NavGrid::get_pathfinder(),
        *transform,
        *movement,
        QVector3D(
            movement->get_requested_goal_x(), 0.0F, movement->get_requested_goal_z()));
    ++facts->progress.repath_count;
    facts->progress.repath_reason =
        Engine::Core::MovementRepathReason::ClearanceChanged;
  }

  facts->route.has_goal = movement->get_has_target();
  facts->route.command_sequence = movement->get_order_sequence();
  facts->route.route_revision = movement->get_route_revision();
  facts->route.requested_goal_x = movement->get_requested_goal_x();
  facts->route.requested_goal_z = movement->get_requested_goal_z();
  facts->route.resolved_goal_x = movement->get_goal_x();
  facts->route.resolved_goal_z = movement->get_goal_y();

  QVector3D const current_pos(transform->position.x, 0.0F, transform->position.z);
  QVector3D const final_goal(movement->get_goal_x(), 0.0F, movement->get_goal_y());
  bool const current_position_allowed = is_movement_point_allowed(current_pos, entity);
  bool const destination_allowed = is_movement_point_allowed(final_goal, entity);

  if (!current_position_allowed &&
      MovementSystem::assign_local_recovery_move(current_pos, final_goal, movement)) {
    facts->progress.state = Engine::Core::MovementOrderState::Recovering;
    facts->progress.repath_reason =
        Engine::Core::MovementRepathReason::RecoveryEscalation;
    return;
  }

  if (movement->get_has_target() && !destination_allowed && current_position_allowed) {
    Point const requested_goal = NavGrid::world_to_grid(final_goal.x(), final_goal.z());
    auto const nearest_goal = NavGrid::find_nearest_walkable_grid(requested_goal, 32);
    if (!nearest_goal.has_value()) {
      facts->progress.state = Engine::Core::MovementOrderState::Unreachable;
      return;
    }

    QVector3D const resolved_goal = NavGrid::grid_to_world(*nearest_goal);
    float const resolved_dx = resolved_goal.x() - final_goal.x();
    float const resolved_dz = resolved_goal.z() - final_goal.z();
    if (resolved_dx * resolved_dx + resolved_dz * resolved_dz >
        k_resolved_goal_progress_epsilon_sq) {
      MovementSystem::retarget_unit(world, entity.get_id(), resolved_goal);
      ++facts->progress.repath_count;
      facts->progress.repath_reason = Engine::Core::MovementRepathReason::GoalChanged;
      facts->progress.state = Engine::Core::MovementOrderState::Repathing;
      return;
    }
  }

  if (!movement->get_has_target()) {
    if (Engine::Core::is_active_movement_state(facts->progress.state)) {
      facts->progress.state = Engine::Core::MovementOrderState::Cancelled;
    }
    return;
  }

  auto const* stamina = entity.get_component<Engine::Core::StaminaComponent>();
  float const max_speed = formation_navigation_speed(entity, *unit, stamina);

  if (movement->has_waypoints()) {
    auto const& waypoint = movement->current_waypoint();
    movement->target_x = waypoint.first;
    movement->target_y = waypoint.second;
  }

  float const waypoint_arrive_radius =
      std::clamp(max_speed * delta_time * 2.0F, 0.05F, 0.25F);
  bool const current_target_is_final =
      !movement->has_waypoints() || movement->remaining_waypoints() <= 1;
  float const arrive_radius =
      current_target_is_final
          ? (movement->get_precise_arrival()
                 ? waypoint_arrive_radius
                 : std::max(waypoint_arrive_radius,
                            std::clamp(CommandService::get_unit_radius(
                                           world, entity.get_id()) *
                                           1.1F,
                                       0.25F,
                                       0.9F)))
          : waypoint_arrive_radius;
  float const arrive_radius_sq = arrive_radius * arrive_radius;

  float dx = movement->get_target_x() - transform->position.x;
  float dz = movement->get_target_y() - transform->position.z;
  float dist2 = dx * dx + dz * dz;

  int safety_counter = k_max_waypoint_skip_count;
  while (movement->get_has_target() && dist2 < arrive_radius_sq &&
         safety_counter-- > 0) {
    if (movement->has_waypoints()) {
      movement->advance_waypoint();
      if (movement->has_waypoints()) {
        auto const& waypoint = movement->current_waypoint();
        movement->target_x = waypoint.first;
        movement->target_y = waypoint.second;
        dx = movement->get_target_x() - transform->position.x;
        dz = movement->get_target_y() - transform->position.z;
        dist2 = dx * dx + dz * dz;
        continue;
      }
    }

    movement->stop();
    OrderService::clear_player_order_intent(&entity);
    facts->progress.state = Engine::Core::MovementOrderState::Arrived;

    auto* guard_mode = entity.get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active &&
        guard_mode->returning_to_guard_position) {
      guard_mode->returning_to_guard_position = false;
    }
    break;
  }

  if (!movement->get_has_target()) {
    return;
  }

  float const distance = std::sqrt(std::max(dist2, 0.0F));
  float const nx = dx / std::max(0.0001F, distance);
  float const nz = dz / std::max(0.0001F, distance);

  float desired_speed = max_speed;
  auto const* move_attack = entity.get_component<Engine::Core::AttackComponent>();
  bool const ranged_mode =
      (move_attack != nullptr) && move_attack->can_ranged &&
      move_attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged;
  float const slow_radius = ranged_mode ? arrive_radius : arrive_radius * 1.5F;
  if (distance < slow_radius) {
    desired_speed = max_speed * (distance / slow_radius);
  }

  facts->desired.valid = true;
  facts->desired.velocity_x = nx * desired_speed;
  facts->desired.velocity_z = nz * desired_speed;
  facts->desired.tangent_x = nx;
  facts->desired.tangent_z = nz;
  facts->desired.lookahead_x = movement->get_target_x();
  facts->desired.lookahead_z = movement->get_target_y();
  facts->desired.speed_limit = max_speed;

  if (!Engine::Core::is_active_movement_state(facts->progress.state) ||
      facts->progress.state == Engine::Core::MovementOrderState::Recovering) {
    facts->progress.state = Engine::Core::MovementOrderState::Following;
  }
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
