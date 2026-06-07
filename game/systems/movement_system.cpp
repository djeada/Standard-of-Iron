#include "movement_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "combat_rules.h"
#include "command_service.h"
#include "core/component.h"
#include "order_service.h"
#include "pathfinding.h"

namespace Game::Systems {

static constexpr int max_waypoint_skip_count = 4;

static constexpr float k_stuck_timeout_seconds = 3.0F;
static constexpr float k_stuck_progress_epsilon_sq = 0.15F * 0.15F;

namespace {

constexpr float hold_mode_turn_speed_degrees = 180.0F;
constexpr float desired_yaw_turn_speed_degrees = 720.0F;

auto should_skip_navigation(const Engine::Core::Entity& entity) -> bool {
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  return commander != nullptr && (commander->jump_active || commander->fpv_controlled);
}

auto max_navigation_speed(const Engine::Core::UnitComponent& unit,
                          const Engine::Core::StaminaComponent* stamina) -> float {
  float speed = std::max(0.1F, unit.speed);
  if (stamina != nullptr && stamina->is_running) {
    speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
  }
  return speed;
}

auto melee_turn_speed_degrees(const Engine::Core::UnitComponent& unit) -> float {
  return Game::Units::is_cavalry(unit.spawn_type) ? 90.0F
                                                  : desired_yaw_turn_speed_degrees;
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
                      const Engine::Core::Entity& entity) -> bool {
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

  return CommandService::is_world_position_walkable(pos);
}

} // namespace

namespace {

void finalize_orientation(Engine::Core::Entity* entity,
                          Engine::Core::TransformComponent* transform,
                          Engine::Core::MovementComponent* movement,
                          float delta_time) {
  auto& terrain = Game::Map::TerrainService::instance();
  if (terrain.is_initialized()) {
    const Game::Map::TerrainHeightMap* hm = terrain.get_height_map();
    if (hm != nullptr) {
      const float tile = hm->get_tile_size();
      const int w = hm->get_width();
      const int h = hm->get_height();
      if (w > 0 && h > 0) {
        const float half_w = w * 0.5F - 0.5F;
        const float half_h = h * 0.5F - 0.5F;
        transform->position.x =
            std::clamp(transform->position.x, -half_w * tile, half_w * tile);
        transform->position.z =
            std::clamp(transform->position.z, -half_h * tile, half_h * tile);
      }
    }
  }

  auto* terrain_ctx = entity->get_component<Engine::Core::TerrainContextComponent>();
  if (terrain_ctx != nullptr && terrain_ctx->audio_cooldown > 0.0F) {
    terrain_ctx->audio_cooldown =
        std::max(0.0F, terrain_ctx->audio_cooldown - delta_time);
  }

  if (entity->has_component<Engine::Core::BuildingComponent>()) {
    return;
  }

  float const speed2 =
      movement->get_vx() * movement->get_vx() + movement->get_vz() * movement->get_vz();
  if (speed2 > 1e-5F) {
    float const target_yaw = std::atan2(movement->get_vx(), movement->get_vz()) *
                             180.0F / std::numbers::pi_v<float>;
    float const current = transform->rotation.y;
    float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
    float const step = std::clamp(diff,
                                  -desired_yaw_turn_speed_degrees * delta_time,
                                  desired_yaw_turn_speed_degrees * delta_time);
    transform->rotation.y = current + step;
  } else if (transform->has_desired_yaw) {
    float const current = transform->rotation.y;
    float const target_yaw = transform->desired_yaw;
    float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
    float const step = std::clamp(diff, -180.0F * delta_time, 180.0F * delta_time);
    transform->rotation.y = current + step;
    if (std::fabs(diff) < 0.5F) {
      transform->has_desired_yaw = false;
    }
  }
}

} // namespace

void MovementSystem::update(Engine::Core::World* world, float delta_time) {
  auto entities = world->get_entities_with<Engine::Core::MovementComponent>();

  for (auto* entity : entities) {
    move_unit(entity, world, delta_time);
  }
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

  if (unit->health <= 0 ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return;
  }

  if (should_skip_navigation(*entity)) {
    if (auto const* commander =
            entity->get_component<Engine::Core::CommanderComponent>();
        commander != nullptr && commander->fpv_controlled && !commander->jump_active) {
      movement->has_target = false;
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
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
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
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

      return;
    }
  }

  if (in_hold_mode) {
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
    if (!entity->has_component<Engine::Core::BuildingComponent>()) {
      apply_desired_yaw(transform, delta_time, melee_turn_speed_degrees(*unit));
    }
    return;
  }

  auto* builder_prod =
      entity->get_component<Engine::Core::BuilderProductionComponent>();
  bool const bypass_mode =
      (builder_prod != nullptr) && builder_prod->bypass_movement_active;

  if (movement->has_target) {
    float const px = transform->position.x;
    float const pz = transform->position.z;
    if (!movement->stuck_ref_valid) {
      movement->stuck_ref_x = px;
      movement->stuck_ref_z = pz;
      movement->stuck_timer = 0.0F;
      movement->stuck_ref_valid = true;
    } else {
      float const moved_x = px - movement->stuck_ref_x;
      float const moved_z = pz - movement->stuck_ref_z;
      if (moved_x * moved_x + moved_z * moved_z > k_stuck_progress_epsilon_sq) {
        movement->stuck_ref_x = px;
        movement->stuck_ref_z = pz;
        movement->stuck_timer = 0.0F;
      } else {
        movement->stuck_timer += delta_time;
        if (movement->stuck_timer >= k_stuck_timeout_seconds) {
          movement->stop();
          OrderService::clear_player_order_intent(entity);
          movement->stuck_ref_valid = false;
          return;
        }
      }
    }
  } else {
    movement->stuck_ref_valid = false;
    movement->stuck_timer = 0.0F;
  }

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
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
    } else {

      float const dist = std::sqrt(std::max(dist_sq, 0.0001F));
      float const nx = dx / dist;
      float const nz = dz / dist;
      float const base_speed = max_navigation_speed(*unit, nullptr);
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

  QVector3D const current_pos_3d(transform->position.x, 0.0F, transform->position.z);
  bool const current_position_allowed = is_point_allowed(current_pos_3d, *entity);
  bool const destination_allowed = is_point_allowed(final_goal, *entity);

  auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
  const float max_speed = max_navigation_speed(*unit, stamina);
  const float accel = max_speed * 4.0F;
  const float damping = 6.0F;

  if (!current_position_allowed && MovementSystem::assign_local_recovery_move(
                                       current_pos_3d, final_goal, movement)) {
    return;
  }

  if (movement->has_target && !destination_allowed && current_position_allowed) {
    Point const requested_goal =
        CommandService::world_to_grid(final_goal.x(), final_goal.z());
    auto const nearest_goal =
        CommandService::find_nearest_walkable_grid(requested_goal, 32);
    if (nearest_goal.has_value()) {
      QVector3D const resolved_goal = CommandService::grid_to_world(*nearest_goal);
      movement->goal_x = resolved_goal.x();
      movement->goal_y = resolved_goal.z();
      MovementSystem::retarget_unit(*world, entity->get_id(), resolved_goal);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    } else {
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
    return;
  }

  if (!movement->has_target) {
    movement->vx *= std::max(0.0F, 1.0F - damping * delta_time);
    movement->vz *= std::max(0.0F, 1.0F - damping * delta_time);
  } else {
    if (movement->has_waypoints()) {
      const auto& wp = movement->current_waypoint();
      movement->target_x = wp.first;
      movement->target_y = wp.second;
    }

    float const waypoint_arrive_radius =
        std::clamp(max_speed * delta_time * 2.0F, 0.05F, 0.25F);
    bool const current_target_is_final =
        !movement->has_waypoints() || movement->remaining_waypoints() <= 1;
    float const arrive_radius =
        current_target_is_final
            ? std::max(
                  waypoint_arrive_radius,
                  std::clamp(CommandService::get_unit_radius(*world, entity->get_id()) *
                                 1.1F,
                             0.25F,
                             0.9F))
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

      movement->stop();
      OrderService::clear_player_order_intent(entity);

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

      auto* const move_attack = entity->get_component<Engine::Core::AttackComponent>();
      bool const ranged_mode = (move_attack != nullptr) && move_attack->can_ranged &&
                               move_attack->current_mode ==
                                   Engine::Core::AttackComponent::CombatMode::Ranged;
      float const slow_radius = ranged_mode ? arrive_radius : arrive_radius * 1.5F;
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

  Point const pre_grid =
      CommandService::world_to_grid(transform->position.x, transform->position.z);
  bool const was_on_valid_tile = CommandService::is_grid_walkable(pre_grid);

  float const old_x = transform->position.x;
  float const old_z = transform->position.z;
  float const new_x = old_x + movement->vx * delta_time;
  float const new_z = old_z + movement->vz * delta_time;

  auto cell_walkable = [](float wx, float wz) -> bool {
    return CommandService::is_grid_walkable(CommandService::world_to_grid(wx, wz));
  };

  if (!was_on_valid_tile || cell_walkable(new_x, new_z)) {

    transform->position.x = new_x;
    transform->position.z = new_z;
  } else {

    bool const x_axis_clear = cell_walkable(new_x, old_z);
    bool const z_axis_clear = cell_walkable(old_x, new_z);

    if (x_axis_clear) {
      transform->position.x = new_x;
      movement->vz = 0.0F;
    } else if (z_axis_clear) {
      transform->position.z = new_z;
      movement->vx = 0.0F;
    } else {

      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
  }

  finalize_orientation(entity, transform, movement, delta_time);
}

} // namespace Game::Systems
