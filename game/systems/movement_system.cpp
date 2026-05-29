#include "movement_system.h"

#include <QVector3D>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "combat_rules.h"
#include "command_service.h"
#include "core/component.h"
#include "core/event_manager.h"
#include "map/terrain.h"
#include "order_service.h"
#include "pathfinding.h"

namespace Game::Systems {

static constexpr int max_waypoint_skip_count = 4;
static constexpr float repath_cooldown_seconds = 0.4F;

namespace {

constexpr float hold_mode_turn_speed_degrees = 180.0F;
constexpr float desired_yaw_turn_speed_degrees = 720.0F;

constexpr float k_stuck_check_dist_sq = 0.01F;
constexpr float k_target_progress_epsilon = 0.05F;
constexpr float k_time_stuck_threshold = 1.5F;
constexpr float k_soft_stuck_push_threshold = 0.20F;
constexpr float k_invalid_tile_recovery_threshold = 0.25F;
constexpr float k_unstuck_cooldown_seconds = 1.5F;
constexpr float k_unstuck_push_seconds = 0.35F;
constexpr float k_unstuck_push_distance = 0.9F;
constexpr int k_max_stuck_recovery_attempts = 1;

class MovementModePolicy {
public:
  virtual ~MovementModePolicy() = default;
  [[nodiscard]] virtual auto
  should_skip_movement(const Engine::Core::Entity&) const -> bool {
    return false;
  }
};

class StandardMovementModePolicy final : public MovementModePolicy {};

class CommanderMovementModePolicy final : public MovementModePolicy {
public:
  [[nodiscard]] auto
  should_skip_movement(const Engine::Core::Entity& entity) const -> bool override {
    auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
    return commander != nullptr &&
           (commander->jump_active || commander->fpv_controlled);
  }
};

auto movement_mode_policy(const Engine::Core::Entity& entity)
    -> const MovementModePolicy& {
  static StandardMovementModePolicy const standard_policy;
  static CommanderMovementModePolicy const commander_policy;
  return entity.has_component<Engine::Core::CommanderComponent>()
             ? static_cast<const MovementModePolicy&>(commander_policy)
             : static_cast<const MovementModePolicy&>(standard_policy);
}

class UnitMovementImplementation {
public:
  virtual ~UnitMovementImplementation() = default;

  [[nodiscard]] virtual auto
  max_speed(const Engine::Core::UnitComponent& unit,
            const Engine::Core::StaminaComponent* stamina) const -> float {
    float speed = std::max(0.1F, unit.speed);
    if (stamina != nullptr && stamina->is_running) {
      speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
    }
    return speed;
  }

  [[nodiscard]] virtual auto movement_turn_speed_degrees() const -> float {
    return desired_yaw_turn_speed_degrees;
  }

  [[nodiscard]] virtual auto melee_turn_speed_degrees() const -> float {
    return desired_yaw_turn_speed_degrees;
  }
};

class HumanoidMovementImplementation final : public UnitMovementImplementation {};

class HorseMovementImplementation final : public UnitMovementImplementation {
public:
  [[nodiscard]] auto melee_turn_speed_degrees() const -> float override {
    return 90.0F;
  }
};

class ElephantMovementImplementation final : public UnitMovementImplementation {};

[[nodiscard]] auto movement_implementation_for(const Engine::Core::UnitComponent& unit)
    -> const UnitMovementImplementation& {
  static HumanoidMovementImplementation const humanoid;
  static HorseMovementImplementation const horse;
  static ElephantMovementImplementation const elephant;

  if (unit.spawn_type == Game::Units::SpawnType::Elephant) {
    return elephant;
  }
  if (Game::Units::is_cavalry(unit.spawn_type)) {
    return horse;
  }
  return humanoid;
}

void apply_desired_yaw(Engine::Core::TransformComponent* transform,
                       float delta_time,
                       float turn_speed_degrees) {
  if ((transform == nullptr) || !transform->has_desired_yaw) {
    return;
  }

  float const current = transform->rotation.y;
  float const target_yaw = transform->desired_yaw;
  float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
  float const step = std::clamp(
      diff, -turn_speed_degrees * delta_time, turn_speed_degrees * delta_time);
  transform->rotation.y = current + step;

  float const remaining_diff =
      std::fmod((target_yaw - transform->rotation.y + 540.0F), 360.0F) - 180.0F;
  if (std::fabs(remaining_diff) < 0.5F) {
    transform->rotation.y = target_yaw;
    transform->has_desired_yaw = false;
  }
}

auto is_point_allowed(const QVector3D& pos,
                      const Engine::Core::Entity& entity,
                      float unit_radius = 0.5F) -> bool {
  if (auto const* builder_prod =
          entity.get_component<Engine::Core::BuilderProductionComponent>();
      builder_prod != nullptr && builder_prod->in_progress &&
      builder_prod->at_construction_site && builder_prod->has_task_target &&
      builder_prod->task_target_id != 0) {
    Point const position_grid = CommandService::world_to_grid(pos.x(), pos.z());
    Point const target_grid = CommandService::world_to_grid(
        builder_prod->task_target_x, builder_prod->task_target_z);
    if (position_grid.x == target_grid.x && position_grid.y == target_grid.y) {
      return true;
    }
  }

  return CommandService::is_world_position_walkable_for_radius(pos, unit_radius);
}

auto is_segment_walkable(const QVector3D& from,
                         const QVector3D& to,
                         Engine::Core::EntityID ignore_entity,
                         float unit_radius = 0.5F) -> bool {
  (void)ignore_entity;
  auto* pathfinder = CommandService::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->update_navigation_grid();
    return pathfinder->is_world_segment_walkable(from, to, unit_radius);
  }

  return CommandService::is_world_position_walkable_for_radius(to, unit_radius);
}

auto try_start_unstuck_push(Engine::Core::Entity* entity,
                            Engine::Core::TransformComponent* transform,
                            Engine::Core::MovementComponent* movement,
                            const QVector3D& preferred_target,
                            float unit_radius,
                            float max_speed) -> bool {
  if (entity == nullptr || transform == nullptr || movement == nullptr) {
    return false;
  }

  QVector3D const current(transform->position.x, 0.0F, transform->position.z);
  QVector3D to_target = preferred_target - current;
  if (to_target.lengthSquared() < 0.0001F) {
    to_target = QVector3D(movement->goal_x, 0.0F, movement->goal_y) - current;
  }
  if (to_target.lengthSquared() < 0.0001F) {
    to_target = QVector3D(1.0F, 0.0F, 0.0F);
  }
  to_target.normalize();

  QVector3D const right(to_target.z(), 0.0F, -to_target.x());
  std::array<QVector3D, 8> const directions{
      to_target,
      right,
      -right,
      -to_target,
      (to_target + right).normalized(),
      (to_target - right).normalized(),
      (-to_target + right).normalized(),
      (-to_target - right).normalized(),
  };

  float const push_step = k_unstuck_push_distance;
  for (const QVector3D& direction : directions) {
    if (direction.lengthSquared() < 0.0001F) {
      continue;
    }
    QVector3D const candidate = current + direction * push_step;
    if (!is_point_allowed(candidate, *entity, unit_radius)) {
      continue;
    }
    if (!is_segment_walkable(current, candidate, entity->get_id(), unit_radius)) {
      continue;
    }

    float const push_speed = std::max(max_speed * 1.2F, 2.0F);
    movement->unstuck_push_vx = direction.x() * push_speed;
    movement->unstuck_push_vz = direction.z() * push_speed;
    movement->unstuck_push_seconds = k_unstuck_push_seconds;
    movement->unstuck_cooldown = std::min(movement->unstuck_cooldown, 0.15F);
    movement->time_stuck = 0.0F;
    return true;
  }

  return false;
}

} // namespace

void MovementSystem::update(Engine::Core::World* world, float delta_time) {
  CommandService::process_repath_requests(*world);
  CommandService::process_path_results(*world);
  prune_moving_units(world);
  auto entities = world->get_entities_with<Engine::Core::MovementComponent>();

  for (auto* entity : entities) {
    move_unit(entity, world, delta_time);
  }
}

void MovementSystem::prune_moving_units(Engine::Core::World* world) {
  if (world == nullptr) {
    m_moving_units.clear();
    m_moving_unit_indices.clear();
    return;
  }

  for (std::size_t idx = m_moving_units.size(); idx > 0; --idx) {
    auto& entry = m_moving_units[idx - 1];
    auto* entity = world->get_entity(entry.entity_id);
    if (entity == nullptr || entity != entry.entity ||
        entity->has_component<Engine::Core::PendingRemovalComponent>() ||
        entity->get_component<Engine::Core::TransformComponent>() == nullptr ||
        entity->get_component<Engine::Core::MovementComponent>() == nullptr) {
      untrack_moving_unit(entry.entity_id);
    }
  }
}

auto MovementSystem::track_moving_unit(
    Engine::Core::Entity* entity,
    const Engine::Core::TransformComponent* transform,
    const Engine::Core::MovementComponent* movement) -> MovingUnitStepState* {
  if (entity == nullptr || transform == nullptr || movement == nullptr) {
    return nullptr;
  }

  auto const entity_id = entity->get_id();
  auto it = m_moving_unit_indices.find(entity_id);
  if (it != m_moving_unit_indices.end()) {
    auto& entry = m_moving_units[it->second];
    entry.entity = entity;
    return &entry;
  }

  float active_target_x = movement->target_x;
  float active_target_z = movement->target_y;
  if (movement->has_waypoints()) {
    const auto& waypoint = movement->current_waypoint();
    active_target_x = waypoint.first;
    active_target_z = waypoint.second;
  }
  float const target_dx = active_target_x - transform->position.x;
  float const target_dz = active_target_z - transform->position.z;
  m_moving_unit_indices[entity_id] = m_moving_units.size();
  m_moving_units.push_back(MovingUnitStepState{
      entity_id,
      entity,
      transform->position.x,
      transform->position.z,
      movement->goal_x,
      movement->goal_y,
      active_target_x,
      active_target_z,
      std::sqrt(target_dx * target_dx + target_dz * target_dz),
      0.0F,
      0,
  });
  return &m_moving_units.back();
}

void MovementSystem::untrack_moving_unit(Engine::Core::EntityID entity_id) {
  auto it = m_moving_unit_indices.find(entity_id);
  if (it == m_moving_unit_indices.end()) {
    return;
  }

  std::size_t const index = it->second;
  std::size_t const last_index = m_moving_units.size() - 1;
  if (index != last_index) {
    m_moving_units[index] = m_moving_units[last_index];
    m_moving_unit_indices[m_moving_units[index].entity_id] = index;
  }
  m_moving_units.pop_back();
  m_moving_unit_indices.erase(it);
}

auto MovementSystem::sample_moving_unit_progress(
    Engine::Core::Entity* entity,
    const Engine::Core::TransformComponent* transform,
    const Engine::Core::MovementComponent* movement,
    bool current_position_allowed,
    float delta_time) -> MovingUnitStepState* {
  if (entity == nullptr || transform == nullptr || movement == nullptr) {
    return nullptr;
  }

  bool const has_movement_activity =
      movement->has_target || movement->has_waypoints() || movement->path_pending ||
      movement->pending_request_id != 0;
  bool const should_watch = has_movement_activity || !current_position_allowed;
  if (!should_watch) {
    untrack_moving_unit(entity->get_id());
    return nullptr;
  }

  auto* entry = track_moving_unit(entity, transform, movement);
  if (entry == nullptr) {
    return nullptr;
  }

  float const goal_dx = movement->goal_x - entry->goal_x;
  float const goal_dz = movement->goal_y - entry->goal_y;
  if (goal_dx * goal_dx + goal_dz * goal_dz > k_stuck_check_dist_sq) {
    entry->goal_x = movement->goal_x;
    entry->goal_y = movement->goal_y;
    entry->last_x = transform->position.x;
    entry->last_z = transform->position.z;
    entry->active_target_x = movement->target_x;
    entry->active_target_z = movement->target_y;
    if (movement->has_waypoints()) {
      const auto& waypoint = movement->current_waypoint();
      entry->active_target_x = waypoint.first;
      entry->active_target_z = waypoint.second;
    }
    float const target_dx = entry->active_target_x - transform->position.x;
    float const target_dz = entry->active_target_z - transform->position.z;
    entry->last_target_distance =
        std::sqrt(target_dx * target_dx + target_dz * target_dz);
    entry->stationary_seconds = 0.0F;
    entry->recovery_attempts = 0;
  }

  bool const should_sample_steps = movement->has_target || !current_position_allowed;
  if (!should_sample_steps) {
    return entry;
  }

  float active_target_x = movement->target_x;
  float active_target_z = movement->target_y;
  if (movement->has_waypoints()) {
    const auto& waypoint = movement->current_waypoint();
    active_target_x = waypoint.first;
    active_target_z = waypoint.second;
  }
  float const active_target_dx = active_target_x - entry->active_target_x;
  float const active_target_dz = active_target_z - entry->active_target_z;
  if (active_target_dx * active_target_dx + active_target_dz * active_target_dz >
      k_stuck_check_dist_sq) {
    float const target_dx = active_target_x - transform->position.x;
    float const target_dz = active_target_z - transform->position.z;
    entry->active_target_x = active_target_x;
    entry->active_target_z = active_target_z;
    entry->last_target_distance =
        std::sqrt(target_dx * target_dx + target_dz * target_dz);
    entry->last_x = transform->position.x;
    entry->last_z = transform->position.z;
    entry->stationary_seconds = 0.0F;
    entry->recovery_attempts = 0;
    return entry;
  }

  float const dpx = transform->position.x - entry->last_x;
  float const dpz = transform->position.z - entry->last_z;
  float const target_dx = active_target_x - transform->position.x;
  float const target_dz = active_target_z - transform->position.z;
  float const distance_to_target =
      std::sqrt(target_dx * target_dx + target_dz * target_dz);
  bool const made_target_progress =
      distance_to_target + k_target_progress_epsilon < entry->last_target_distance;
  if (made_target_progress) {
    entry->last_x = transform->position.x;
    entry->last_z = transform->position.z;
    entry->last_target_distance = distance_to_target;
    entry->stationary_seconds = 0.0F;
    entry->recovery_attempts = 0;
  } else {
    entry->stationary_seconds += delta_time;
  }

  return entry;
}

void MovementSystem::move_unit(Engine::Core::Entity* entity,
                               Engine::Core::World* world,
                               float delta_time) {
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();

  if ((transform == nullptr) || (movement == nullptr) || (unit == nullptr)) {
    return;
  }
  const UnitMovementImplementation& movement_impl = movement_implementation_for(*unit);

  if (unit->health <= 0 ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    untrack_moving_unit(entity->get_id());
    return;
  }

  if (movement_mode_policy(*entity).should_skip_movement(*entity)) {
    if (auto const* commander =
            entity->get_component<Engine::Core::CommanderComponent>();
        commander != nullptr && commander->fpv_controlled && !commander->jump_active) {
      movement->has_target = false;
      OrderService::clear_player_order_intent(entity);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
    untrack_moving_unit(entity->get_id());
    return;
  }

  auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  bool in_hold_mode = false;
  if (hold_mode != nullptr) {
    if (hold_mode->exit_cooldown > 0.0F) {
      hold_mode->exit_cooldown = std::max(0.0F, hold_mode->exit_cooldown - delta_time);
    }

    if (hold_mode->active) {
      movement->has_target = false;
      OrderService::clear_player_order_intent(entity);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
      untrack_moving_unit(entity->get_id());
      in_hold_mode = true;

      if (hold_mode->kneel_duration > 0.0F && hold_mode->kneel_entry_progress < 1.0F) {
        hold_mode->kneel_entry_progress = std::min(
            1.0F,
            hold_mode->kneel_entry_progress + delta_time / hold_mode->kneel_duration);
      }
    } else {
      hold_mode->kneel_entry_progress = 0.0F;
    }

    if (hold_mode->exit_cooldown > 0.0F && !in_hold_mode) {
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      untrack_moving_unit(entity->get_id());

      return;
    }
  }

  if (in_hold_mode) {
    untrack_moving_unit(entity->get_id());
    if (!entity->has_component<Engine::Core::BuildingComponent>()) {
      apply_desired_yaw(transform, delta_time, hold_mode_turn_speed_degrees);
    }
    return;
  }

  auto* atk = entity->get_component<Engine::Core::AttackComponent>();
  if ((atk != nullptr) && atk->in_melee_lock &&
      CombatRules::participates_in_rts_melee_lock(entity)) {
    movement->has_target = false;
    OrderService::clear_player_order_intent(entity);
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    movement->clear_path();
    movement->path_pending = false;
    untrack_moving_unit(entity->get_id());
    if (!entity->has_component<Engine::Core::BuildingComponent>()) {
      apply_desired_yaw(
          transform, delta_time, movement_impl.melee_turn_speed_degrees());
    }
    return;
  }

  auto* builder_prod =
      entity->get_component<Engine::Core::BuilderProductionComponent>();
  bool const bypass_mode =
      (builder_prod != nullptr) && builder_prod->bypass_movement_active;

  if (bypass_mode) {

    float const dx = builder_prod->bypass_target_x - transform->position.x;
    float const dz = builder_prod->bypass_target_z - transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    constexpr float k_bypass_arrival_dist_sq = 0.25F;

    if (dist_sq < k_bypass_arrival_dist_sq) {

      transform->position.x = builder_prod->bypass_target_x;
      transform->position.z = builder_prod->bypass_target_z;
      builder_prod->bypass_movement_active = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->has_target = false;
      OrderService::clear_player_order_intent(entity);
      movement->clear_path();
      untrack_moving_unit(entity->get_id());
    } else {

      float const dist = std::sqrt(std::max(dist_sq, 0.0001F));
      float const nx = dx / dist;
      float const nz = dz / dist;
      float const base_speed = movement_impl.max_speed(*unit, nullptr);
      movement->vx = nx * base_speed;
      movement->vz = nz * base_speed;

      transform->position.x += movement->vx * delta_time;
      transform->position.z += movement->vz * delta_time;

      float const target_yaw =
          std::atan2(movement->vx, movement->vz) * 180.0F / std::numbers::pi_v<float>;
      float const current = transform->rotation.y;
      float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = 720.0F;
      float const step =
          std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
      transform->rotation.y = current + step;
    }
    return;
  }

  QVector3D const final_goal(movement->goal_x, 0.0F, movement->goal_y);

  float const unit_radius = CommandService::get_unit_radius(*world, entity->get_id());

  QVector3D const current_pos_3d(transform->position.x, 0.0F, transform->position.z);
  bool const current_position_allowed =
      is_point_allowed(current_pos_3d, *entity, unit_radius);
  bool const destination_allowed = is_point_allowed(final_goal, *entity, unit_radius);

  if (current_position_allowed) {
    movement->time_on_invalid_tile = 0.0F;
  } else {
    movement->time_on_invalid_tile += delta_time;
  }

  if (movement->unstuck_cooldown > 0.0F) {
    movement->unstuck_cooldown =
        std::max(0.0F, movement->unstuck_cooldown - delta_time);
  }

  auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
  const float max_speed = movement_impl.max_speed(*unit, stamina);
  const float accel = max_speed * 4.0F;
  const float damping = 6.0F;

  auto* moving_step_state = sample_moving_unit_progress(
      entity, transform, movement, current_position_allowed, delta_time);
  if (moving_step_state != nullptr) {
    movement->last_position_x = moving_step_state->last_x;
    movement->last_position_z = moving_step_state->last_z;
    movement->time_stuck = moving_step_state->stationary_seconds;
  } else {
    movement->last_position_x = transform->position.x;
    movement->last_position_z = transform->position.z;
    movement->time_stuck = 0.0F;
  }

  bool const needs_recovery = !movement->path_pending &&
                              movement->repath_cooldown <= 0.0F &&
                              !current_position_allowed;
  bool const invalid_position_persistent =
      movement->time_on_invalid_tile >= k_invalid_tile_recovery_threshold;
  bool const persistent_invalid_position_recovery = !movement->path_pending &&
                                                    movement->repath_cooldown <= 0.0F &&
                                                    invalid_position_persistent;
  bool const has_no_valid_target = !movement->has_target || !destination_allowed;

  bool const force_recovery =
      !current_position_allowed && !movement->path_pending &&
      movement->unstuck_cooldown <= 0.0F && moving_step_state != nullptr &&
      moving_step_state->stationary_seconds >= k_time_stuck_threshold;
  bool const valid_position_stuck =
      current_position_allowed && movement->has_target && !movement->path_pending &&
      movement->unstuck_cooldown <= 0.0F && moving_step_state != nullptr &&
      moving_step_state->stationary_seconds >= k_time_stuck_threshold;

  if (current_position_allowed && movement->has_target && !movement->path_pending &&
      movement->unstuck_push_seconds <= 0.0F && moving_step_state != nullptr &&
      moving_step_state->stationary_seconds >= k_soft_stuck_push_threshold &&
      moving_step_state->stationary_seconds < k_time_stuck_threshold) {
    QVector3D push_target(movement->target_x, 0.0F, movement->target_y);
    if (movement->has_waypoints()) {
      const auto& wp = movement->current_waypoint();
      push_target = QVector3D(wp.first, 0.0F, wp.second);
    }
    if (CommandService::try_queue_local_recovery_move(
            *world, entity->get_id(), current_pos_3d, final_goal, movement)) {
      moving_step_state->stationary_seconds = 0.0F;
      ++moving_step_state->recovery_attempts;
      movement->repath_cooldown = repath_cooldown_seconds;
      return;
    }
    if (try_start_unstuck_push(
            entity, transform, movement, push_target, unit_radius, max_speed)) {
      moving_step_state->stationary_seconds = 0.0F;
      ++moving_step_state->recovery_attempts;
      return;
    }
  }

  if (valid_position_stuck) {
    QVector3D push_target(movement->target_x, 0.0F, movement->target_y);
    if (movement->has_waypoints()) {
      const auto& wp = movement->current_waypoint();
      push_target = QVector3D(wp.first, 0.0F, wp.second);
    }
    if (try_start_unstuck_push(
            entity, transform, movement, push_target, unit_radius, max_speed)) {
      moving_step_state->stationary_seconds = 0.0F;
      ++moving_step_state->recovery_attempts;
      return;
    }

    if (moving_step_state->recovery_attempts < k_max_stuck_recovery_attempts &&
        destination_allowed) {
      CommandService::queue_repath_request(*world, entity->get_id(), final_goal, false);
      movement->clear_path();
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->repath_cooldown = repath_cooldown_seconds;
      movement->time_stuck = 0.0F;
      movement->unstuck_cooldown = k_unstuck_cooldown_seconds;
      moving_step_state->stationary_seconds = 0.0F;
      ++moving_step_state->recovery_attempts;
      return;
    }
    movement->clear_path();
    movement->has_target = false;
    OrderService::clear_player_order_intent(entity);
    movement->path_pending = false;
    movement->pending_request_id = 0;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    movement->time_stuck = 0.0F;
    movement->unstuck_cooldown = k_unstuck_cooldown_seconds;
    untrack_moving_unit(entity->get_id());
    return;
  }

  if (((needs_recovery && has_no_valid_target) ||
       persistent_invalid_position_recovery || force_recovery) &&
      CommandService::try_queue_local_recovery_move(
          *world, entity->get_id(), current_pos_3d, final_goal, movement)) {
    movement->repath_cooldown = repath_cooldown_seconds;
    movement->time_on_invalid_tile = 0.0F;
    if (force_recovery || persistent_invalid_position_recovery) {
      movement->time_stuck = 0.0F;
      movement->unstuck_cooldown = k_unstuck_cooldown_seconds;
      if (moving_step_state != nullptr) {
        moving_step_state->stationary_seconds = 0.0F;
        moving_step_state->recovery_attempts = 0;
      }
    }
  }

  if (movement->has_target && !destination_allowed && current_position_allowed) {
    movement->clear_path();
    movement->has_target = false;
    OrderService::clear_player_order_intent(entity);
    movement->path_pending = false;
    movement->pending_request_id = 0;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    untrack_moving_unit(entity->get_id());
    return;
  }

  if (movement->repath_cooldown > 0.0F) {
    movement->repath_cooldown = std::max(0.0F, movement->repath_cooldown - delta_time);
  }

  if (movement->time_since_last_path_request < 10.0F) {
    movement->time_since_last_path_request += delta_time;
  }

  bool const applying_unstuck_push = movement->unstuck_push_seconds > 0.0F;
  if (applying_unstuck_push) {
    movement->vx = movement->unstuck_push_vx;
    movement->vz = movement->unstuck_push_vz;
    movement->unstuck_push_seconds =
        std::max(0.0F, movement->unstuck_push_seconds - delta_time);
    if (movement->unstuck_push_seconds <= 0.0F) {
      movement->unstuck_push_vx = 0.0F;
      movement->unstuck_push_vz = 0.0F;
    }
  } else if (!movement->has_target) {
    QVector3D const current_pos(transform->position.x, 0.0F, transform->position.z);
    float const goal_dist_sq = (final_goal - current_pos).lengthSquared();
    constexpr float k_stuck_distance_sq = 0.6F * 0.6F;

    bool requested_recovery_move = false;
    if (!movement->path_pending && movement->repath_cooldown <= 0.0F &&
        goal_dist_sq > k_stuck_distance_sq && std::isfinite(goal_dist_sq) &&
        destination_allowed) {
      CommandService::queue_repath_request(*world, entity->get_id(), final_goal, true);
      movement->repath_cooldown = repath_cooldown_seconds;
      requested_recovery_move = true;
    }

    if (!requested_recovery_move) {
      movement->vx *= std::max(0.0F, 1.0F - damping * delta_time);
      movement->vz *= std::max(0.0F, 1.0F - damping * delta_time);
    }
  } else {
    QVector3D current_pos(transform->position.x, 0.0F, transform->position.z);
    QVector3D segment_target(movement->target_x, 0.0F, movement->target_y);
    if (movement->has_waypoints()) {
      const auto& wp = movement->current_waypoint();
      segment_target = QVector3D(wp.first, 0.0F, wp.second);
    }
    auto refresh_segment_target = [&]() {
      if (movement->has_waypoints()) {
        const auto& wp = movement->current_waypoint();
        movement->target_x = wp.first;
        movement->target_y = wp.second;
        segment_target = QVector3D(movement->target_x, 0.0F, movement->target_y);
      } else {
        segment_target = QVector3D(movement->target_x, 0.0F, movement->target_y);
      }
    };

    auto try_advance_past_blocked_segment = [&]() {
      bool recovered = false;
      int skips_remaining = max_waypoint_skip_count;
      while (movement->has_waypoints() && skips_remaining-- > 0) {
        movement->advance_waypoint();
        refresh_segment_target();
        if (is_segment_walkable(
                current_pos, segment_target, entity->get_id(), unit_radius)) {
          recovered = true;
          break;
        }
      }

      if (!recovered && !movement->has_waypoints()) {
        refresh_segment_target();
        if (is_segment_walkable(
                current_pos, segment_target, entity->get_id(), unit_radius)) {
          recovered = true;
        }
      }

      return recovered;
    };

    bool const escaping_invalid_ground = !current_position_allowed;
    bool const destination_tile_walkable = [&]() -> bool {
      Point const d =
          CommandService::world_to_grid(segment_target.x(), segment_target.z());
      return CommandService::is_grid_walkable_for_radius(d, unit_radius);
    }();

    if (!(escaping_invalid_ground && destination_tile_walkable) &&
        !is_segment_walkable(
            current_pos, segment_target, entity->get_id(), unit_radius)) {
      if (movement->unstuck_push_seconds <= 0.0F &&
          try_start_unstuck_push(
              entity, transform, movement, segment_target, unit_radius, max_speed)) {
        if (moving_step_state != nullptr) {
          moving_step_state->stationary_seconds = 0.0F;
          moving_step_state->recovery_attempts = 0;
        }
        return;
      }

      if (movement->path_pending || movement->pending_request_id != 0) {
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        return;
      }

      if (!try_advance_past_blocked_segment()) {
        bool issued_path_request = false;
        float const goal_dist_sq = (final_goal - current_pos).lengthSquared();
        if (!movement->path_pending && movement->repath_cooldown <= 0.0F) {
          if (goal_dist_sq > 0.01F && destination_allowed) {
            CommandService::queue_repath_request(
                *world,
                entity->get_id(),
                QVector3D(movement->goal_x, 0.0F, movement->goal_y),
                false);
            movement->repath_cooldown = repath_cooldown_seconds;
            issued_path_request = true;
          }
        }

        if (!issued_path_request && movement->repath_cooldown > 0.0F &&
            goal_dist_sq > 0.01F && destination_allowed) {
          movement->vx = 0.0F;
          movement->vz = 0.0F;
          return;
        }

        if (!issued_path_request) {
          movement->path_pending = false;
          movement->pending_request_id = 0;
        }

        movement->clear_path();
        movement->has_target = false;
        OrderService::clear_player_order_intent(entity);
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        if (!issued_path_request) {
          untrack_moving_unit(entity->get_id());
        }
        return;
      }
    }

    float const waypoint_arrive_radius =
        std::clamp(max_speed * delta_time * 2.0F, 0.05F, 0.25F);
    bool const current_target_is_final =
        !movement->has_waypoints() || movement->remaining_waypoints() <= 1;
    float const arrive_radius =
        current_target_is_final ? std::max(waypoint_arrive_radius,
                                           std::clamp(unit_radius * 1.1F, 0.25F, 0.9F))
                                : waypoint_arrive_radius;
    float const arrive_radius_sq = arrive_radius * arrive_radius;

    float dx = movement->target_x - transform->position.x;
    float dz = movement->target_y - transform->position.z;
    float dist2 = dx * dx + dz * dz;

    int safety_counter = max_waypoint_skip_count;
    while (movement->has_target && dist2 < arrive_radius_sq && safety_counter-- > 0) {
      if (movement->has_waypoints()) {
        movement->advance_waypoint();
        if (movement->has_waypoints()) {
          const auto& wp = movement->current_waypoint();
          movement->target_x = wp.first;
          movement->target_y = wp.second;
          dx = movement->target_x - transform->position.x;
          dz = movement->target_y - transform->position.z;
          dist2 = dx * dx + dz * dz;
          continue;
        }
      }

      movement->has_target = false;
      OrderService::clear_player_order_intent(entity);
      movement->vx = movement->vz = 0.0F;
      untrack_moving_unit(entity->get_id());

      auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active &&
          guard_mode->returning_to_guard_position) {
        guard_mode->returning_to_guard_position = false;
      }

      break;
    }

    if (!movement->has_target) {
      movement->vx *= std::max(0.0F, 1.0F - damping * delta_time);
      movement->vz *= std::max(0.0F, 1.0F - damping * delta_time);
    } else {
      float const distance = std::sqrt(std::max(dist2, 0.0F));
      float const nx = dx / std::max(0.0001F, distance);
      float const nz = dz / std::max(0.0001F, distance);
      float desired_speed = max_speed;
      float const slow_radius = arrive_radius * 4.0F;
      if (distance < slow_radius) {
        desired_speed = max_speed * (distance / slow_radius);
      }

      float const desired_vx = nx * desired_speed;
      float const desired_vz = nz * desired_speed;

      float const ax = (desired_vx - movement->vx) * accel;
      float const az = (desired_vz - movement->vz) * accel;
      movement->vx += ax * delta_time;
      movement->vz += az * delta_time;

      movement->vx *= std::max(0.0F, 1.0F - 0.5F * damping * delta_time);
      movement->vz *= std::max(0.0F, 1.0F - 0.5F * damping * delta_time);
    }
  }

  bool was_on_valid_tile = true;
  Point const pre_grid =
      CommandService::world_to_grid(transform->position.x, transform->position.z);
  was_on_valid_tile =
      CommandService::is_grid_walkable_for_radius(pre_grid, unit_radius);

  transform->position.x += movement->vx * delta_time;
  transform->position.z += movement->vz * delta_time;

  auto& terrain = Game::Map::TerrainService::instance();
  if (auto* pathfinder = CommandService::get_pathfinder(); pathfinder != nullptr) {
    QVector3D const projected =
        pathfinder->project_world_position_to_traversal_corridor(QVector3D(
            transform->position.x, transform->position.y, transform->position.z));
    transform->position.x = projected.x();
    transform->position.z = projected.z();
  }

  {
    Point const new_grid =
        CommandService::world_to_grid(transform->position.x, transform->position.z);
    if (!CommandService::is_grid_walkable_for_radius(new_grid, unit_radius)) {
      if (was_on_valid_tile) {

        transform->position.x -= movement->vx * delta_time;
        transform->position.z -= movement->vz * delta_time;

        movement->vx = 0.0F;
        movement->vz = 0.0F;
        movement->has_target = false;
        OrderService::clear_player_order_intent(entity);
        movement->clear_path();
        movement->path_pending = false;

        QVector3D const reverted_pos(
            transform->position.x, 0.0F, transform->position.z);
        QVector3D const goal(movement->goal_x, 0.0F, movement->goal_y);
        bool recovered_locally = false;
        if (movement->goal_x != 0.0F || movement->goal_y != 0.0F) {
          recovered_locally = CommandService::try_queue_local_recovery_move(
              *world, entity->get_id(), reverted_pos, goal, movement);
        }

        if (!recovered_locally &&
            (movement->goal_x != 0.0F || movement->goal_y != 0.0F)) {
          Point const goal_grid =
              CommandService::world_to_grid(movement->goal_x, movement->goal_y);
          if (CommandService::is_grid_walkable_for_radius(goal_grid, unit_radius)) {
            CommandService::queue_repath_request(
                *world,
                entity->get_id(),
                QVector3D(movement->goal_x, 0.0F, movement->goal_y),
                false);
          }
        }
      }
    }
  }

  if (!movement->has_target && !movement->has_waypoints() && !movement->path_pending &&
      movement->pending_request_id == 0) {
    untrack_moving_unit(entity->get_id());
  }

  if (terrain.is_initialized()) {
    const Game::Map::TerrainHeightMap* hm = terrain.get_height_map();
    if (hm != nullptr) {
      const float tile = hm->get_tile_size();
      const int w = hm->get_width();
      const int h = hm->get_height();
      if (w > 0 && h > 0) {
        const float half_w = w * 0.5F - 0.5F;
        const float half_h = h * 0.5F - 0.5F;
        const float min_x = -half_w * tile;
        const float max_x = half_w * tile;
        const float min_z = -half_h * tile;
        const float max_z = half_h * tile;
        transform->position.x = std::clamp(transform->position.x, min_x, max_x);
        transform->position.z = std::clamp(transform->position.z, min_z, max_z);
      }
    }
  }

  auto* terrain_ctx = entity->get_component<Engine::Core::TerrainContextComponent>();
  if (terrain_ctx != nullptr && terrain_ctx->audio_cooldown > 0.0F) {
    terrain_ctx->audio_cooldown =
        std::max(0.0F, terrain_ctx->audio_cooldown - delta_time);
  }

  if (!entity->has_component<Engine::Core::BuildingComponent>()) {
    float const speed2 = movement->vx * movement->vx + movement->vz * movement->vz;
    bool const is_moving = speed2 > 1e-5F;
    if (is_moving) {
      float const target_yaw =
          std::atan2(movement->vx, movement->vz) * 180.0F / std::numbers::pi_v<float>;

      float const current = transform->rotation.y;

      float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = movement_impl.movement_turn_speed_degrees();
      float const step =
          std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
      transform->rotation.y = current + step;
    } else if (transform->has_desired_yaw) {

      float const current = transform->rotation.y;
      float const target_yaw = transform->desired_yaw;
      float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = 180.0F;
      float const step =
          std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
      transform->rotation.y = current + step;

      if (std::fabs(diff) < 0.5F) {
        transform->has_desired_yaw = false;
      }
    }
  }
}

} // namespace Game::Systems
