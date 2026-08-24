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

constexpr float k_resolved_goal_progress_epsilon_sq = 0.01F;
constexpr float k_clearance_repath_threshold = 0.25F;

constexpr float k_progress_window_metres = 0.03F;

constexpr float k_launch_grace_seconds = 0.60F;

constexpr float k_yield_budget_seconds = 8.0F;
constexpr float k_block_declare_seconds = 0.35F;

constexpr float k_block_escalate_seconds = 0.40F;
constexpr float k_repath_settle_seconds = 0.60F;
constexpr float k_recovery_budget_seconds = 1.50F;
constexpr std::uint32_t k_max_repath_attempts = 3U;

constexpr float k_lookahead_speed_seconds = 0.35F;
constexpr float k_lookahead_min = 0.45F;
constexpr float k_lookahead_max = 2.0F;
constexpr float k_projection_window_min = 1.5F;
constexpr float k_degenerate_aim_distance = 0.15F;

constexpr std::uint64_t k_route_prune_interval_ticks = 600U;

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
  float speed = max_navigation_speed(unit, stamina) *
                DefensiveUnitLayoutService::move_speed_multiplier(entity) *
                Game::Formation::ArmyFormationRuntime::move_speed_multiplier(entity);
  const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
  if (movement != nullptr && movement->get_declared_group_pace() > 0.0F) {
    speed = std::min(speed, movement->get_declared_group_pace());
  }
  return speed;
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
  facts->route.route_id = movement->get_route_id();
  facts->route.route_revision = movement->get_route_revision();
  facts->route.topology_revision = movement->get_topology_revision();
  facts->route.lane_offset = movement->get_route_lane_offset();
  facts->route.lane_scale = movement->get_route_lane_scale();
  facts->route.cohesion_pace = 0.0F;
  const auto* membership =
      entity.get_component<Engine::Core::ArmyFormationMembershipComponent>();
  if (membership != nullptr && membership->is_valid()) {
    const auto* formation =
        Game::Formation::ArmyFormationRegistry::instance().find(membership->group_id);
    if (formation != nullptr) {
      facts->route.cohesion_pace = formation->cohesion_pace;
    }
  }
  if (facts->route.cohesion_pace <= 0.0F) {
    facts->route.cohesion_pace = movement->get_declared_group_pace();
  }
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

  auto& route = m_routes[entity.get_id()];
  bool route_changed = false;
  if (!route.valid() || route.route_revision() != movement->get_route_revision()) {
    route_changed = true;

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
    if (std::hypot(last.first - final_x, last.second - final_z) > 1.0e-4F) {
      route.update_final_point(last.first, last.second);
    }
  }

  float const waypoint_arrive_radius =
      std::clamp(max_speed * delta_time * 2.0F, 0.05F, 0.25F);
  float const arrive_radius =
      movement->get_precise_arrival()
          ? waypoint_arrive_radius
          : std::max(waypoint_arrive_radius,
                     std::clamp(
                         CommandService::get_unit_radius(world, entity.get_id()) * 1.1F,
                         0.25F,
                         0.9F));

  float aim_x = movement->get_target_x();
  float aim_z = movement->get_target_y();
  float endpoint_x = movement->get_target_x();
  float endpoint_z = movement->get_target_y();
  float remaining = 0.0F;
  float tangent_x = 0.0F;
  float tangent_z = 0.0F;

  if (route.valid()) {
    float const window =
        std::max(k_projection_window_min,
                 max_speed * delta_time * 8.0F + movement->get_navigation_clearance());
    auto const projection =
        route.project(transform->position.x, transform->position.z, window);
    route.advance_to(projection.s);
    float const s = route.travelled();
    remaining = route.remaining();
    facts->progress.lateral_route_error = projection.lateral;

    std::size_t const target_index = route.waypoint_index_at(s);
    while (movement->get_path_index() < target_index && movement->has_waypoints()) {
      movement->advance_waypoint();
    }
    auto const final_point = route.final_point();
    endpoint_x = final_point.first;
    endpoint_z = final_point.second;
    if (movement->has_waypoints()) {
      auto const& waypoint = movement->current_waypoint();
      movement->target_x = waypoint.first;
      movement->target_y = waypoint.second;
    } else {
      movement->target_x = endpoint_x;
      movement->target_y = endpoint_z;
    }

    float const lookahead = std::clamp(
        max_speed * k_lookahead_speed_seconds, k_lookahead_min, k_lookahead_max);
    float aim_s = std::min(s + lookahead, route.next_vertex_s(s));
    auto aim = route.point_at(aim_s);
    if (std::hypot(aim.first - transform->position.x,
                   aim.second - transform->position.z) < k_degenerate_aim_distance) {
      aim_s = std::min(route.length(), route.next_vertex_s(aim_s));
      aim = route.point_at(aim_s);
    }
    aim_x = aim.first;
    aim_z = aim.second;

    auto const tangent = route.tangent_at(s);
    tangent_x = tangent.first;
    tangent_z = tangent.second;
  } else {

    remaining = std::hypot(endpoint_x - transform->position.x,
                           endpoint_z - transform->position.z);
    facts->progress.lateral_route_error = 0.0F;
  }

  if (!update_progress(entity,
                       world,
                       *transform,
                       *movement,
                       *facts,
                       remaining,
                       route_changed,
                       delta_time)) {
    return;
  }

  float const endpoint_distance = std::hypot(endpoint_x - transform->position.x,
                                             endpoint_z - transform->position.z);
  if (remaining <= arrive_radius && endpoint_distance <= arrive_radius) {
    movement->stop();
    OrderService::clear_player_order_intent(&entity);
    facts->progress.state = Engine::Core::MovementOrderState::Arrived;
    facts->progress.no_progress_seconds = 0.0F;
    facts->progress.no_progress_advance = 0.0F;
    facts->progress.remaining_arclength = 0.0F;
    route.clear();

    auto* guard_mode = entity.get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active &&
        guard_mode->returning_to_guard_position) {
      guard_mode->returning_to_guard_position = false;
    }
    return;
  }

  float const dx = aim_x - transform->position.x;
  float const dz = aim_z - transform->position.z;
  float const distance = std::hypot(dx, dz);
  float const nx = dx / std::max(0.0001F, distance);
  float const nz = dz / std::max(0.0001F, distance);
  if (tangent_x == 0.0F && tangent_z == 0.0F) {
    tangent_x = nx;
    tangent_z = nz;
  }

  float desired_speed = max_speed;
  auto const* move_attack = entity.get_component<Engine::Core::AttackComponent>();
  bool const ranged_mode =
      (move_attack != nullptr) && move_attack->can_ranged &&
      move_attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged;
  float const slow_radius = ranged_mode ? arrive_radius : arrive_radius * 1.5F;
  if (remaining < slow_radius) {
    desired_speed = max_speed * (remaining / slow_radius);
  }

  facts->desired.valid = true;
  facts->desired.velocity_x = nx * desired_speed;
  facts->desired.velocity_z = nz * desired_speed;
  facts->desired.tangent_x = tangent_x;
  facts->desired.tangent_z = tangent_z;
  facts->desired.lookahead_x = aim_x;
  facts->desired.lookahead_z = aim_z;
  facts->desired.speed_limit = max_speed;
}

auto RouteFollowSystem::route_for(Engine::Core::EntityID entity_id) const
    -> const MovementRoute* {
  auto const found = m_routes.find(entity_id);
  return found == m_routes.end() ? nullptr : &found->second;
}

auto RouteFollowSystem::update_progress(Engine::Core::Entity& entity,
                                        Engine::Core::World& world,
                                        Engine::Core::TransformComponent& transform,
                                        Engine::Core::MovementComponent& movement,
                                        Engine::Core::MovementFactsComponent& facts,
                                        float remaining,
                                        bool route_changed,
                                        float delta_time) -> bool {
  using Engine::Core::MovementOrderState;
  using Engine::Core::MovementRepathReason;

  auto& progress = facts.progress;
  MovementOrderState const entry_state = progress.state;

  float const previous_remaining = progress.remaining_arclength;
  bool const comparable = !route_changed && previous_remaining > 0.0F;
  float const advance = comparable ? previous_remaining - remaining : 0.0F;
  progress.route_advance = advance;
  progress.remaining_arclength = remaining;

  if (progress.tracked_order != movement.get_order_sequence()) {
    progress.tracked_order = movement.get_order_sequence();
    progress.order_seconds = 0.0F;
    progress.no_progress_seconds = 0.0F;
    progress.no_progress_advance = 0.0F;
    progress.repath_attempts = 0;
  }
  progress.order_seconds += delta_time;

  bool const yielding_to_traffic =
      facts.steering.valid &&
      (facts.steering.result == Engine::Core::SteeringResult::Yielded ||
       facts.steering.result == Engine::Core::SteeringResult::Separating) &&
      progress.state != MovementOrderState::Recovering;

  progress.no_progress_seconds += delta_time;
  progress.no_progress_advance += std::max(0.0F, advance);
  if (route_changed || progress.no_progress_advance >= k_progress_window_metres) {
    progress.no_progress_seconds = 0.0F;
    progress.no_progress_advance = 0.0F;
    progress.repath_attempts = 0;
    if (progress.state != MovementOrderState::Yielding) {
      progress.state = MovementOrderState::Following;
    }
  }

  if (yielding_to_traffic && (progress.state == MovementOrderState::Following ||
                              progress.state == MovementOrderState::Turning ||
                              progress.state == MovementOrderState::Yielding)) {
    progress.state = MovementOrderState::Yielding;
  }

  switch (progress.state) {
  case MovementOrderState::Following:
  case MovementOrderState::Turning:
    if (progress.order_seconds > k_launch_grace_seconds &&
        progress.no_progress_seconds > k_block_declare_seconds) {
      progress.state = MovementOrderState::LocallyBlocked;
    }
    break;

  case MovementOrderState::Yielding:
    if (!yielding_to_traffic) {
      progress.state = MovementOrderState::Following;
    } else if (progress.state_seconds > k_yield_budget_seconds) {
      progress.state = MovementOrderState::LocallyBlocked;
    }
    break;

  case MovementOrderState::LocallyBlocked:
    if (progress.state_seconds > k_block_escalate_seconds) {
      if (progress.repath_attempts >= k_max_repath_attempts) {
        progress.state = MovementOrderState::Recovering;
        progress.repath_reason = MovementRepathReason::RecoveryEscalation;
      } else {
        QVector3D const goal =
            movement.get_has_requested_goal()
                ? QVector3D(movement.get_requested_goal_x(),
                            0.0F,
                            movement.get_requested_goal_z())
                : QVector3D(movement.get_goal_x(), 0.0F, movement.get_goal_y());
        MovementSystem::retarget_unit(world, entity.get_id(), goal);
        ++progress.repath_count;
        ++progress.repath_attempts;
        progress.repath_reason = MovementRepathReason::Blocked;
        progress.state = MovementOrderState::Repathing;
      }
    }
    break;

  case MovementOrderState::Repathing:
    if (progress.state_seconds > k_repath_settle_seconds) {
      progress.state = MovementOrderState::LocallyBlocked;
    }
    break;

  case MovementOrderState::Recovering: {
    QVector3D const current(transform.position.x, 0.0F, transform.position.z);
    QVector3D const goal(movement.get_goal_x(), 0.0F, movement.get_goal_y());
    if (progress.state_seconds <= k_recovery_budget_seconds) {
      MovementSystem::assign_local_recovery_move(current, goal, &movement);
      break;
    }
    movement.stop();
    OrderService::clear_player_order_intent(&entity);
    progress.state = MovementOrderState::Unreachable;
    progress.no_progress_seconds = 0.0F;
    progress.no_progress_advance = 0.0F;
    progress.remaining_arclength = 0.0F;
    m_routes.erase(entity.get_id());
    break;
  }

  default:
    progress.state = MovementOrderState::Following;
    break;
  }

  movement.stuck_timer = progress.no_progress_seconds;
  movement.stuck_ref_valid = false;

  progress.previous_state = entry_state;
  progress.state_seconds =
      progress.state == entry_state ? progress.state_seconds + delta_time : 0.0F;

  return movement.get_has_target();
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
