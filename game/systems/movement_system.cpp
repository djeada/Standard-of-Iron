#include "movement_system.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "core/component.h"
#include "map/terrain.h"
#include "pathfinding.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <qvectornd.h>
#include <random>
#include <vector>

namespace Game::Systems {

static constexpr int max_waypoint_skip_count = 4;
static constexpr float repath_cooldown_seconds = 0.4F;
static constexpr int kNearestPointSearchRadius = 5;

// Deadlock prevention constants
static constexpr float kStuckDetectionThreshold = 0.1F;  // Distance threshold to consider stuck
static constexpr float kStuckTimeThreshold = 2.0F;       // Time stuck before recovery kicks in
static constexpr float kUnstuckCooldown = 1.5F;          // Cooldown between unstuck attempts
static constexpr float kUnstuckOffsetRadius = 1.0F;      // Random offset radius for unstuck

namespace {

auto is_point_allowed(const QVector3D &pos, Engine::Core::EntityID ignoreEntity,
                      float unit_radius = 0.5F) -> bool {
  auto &registry = BuildingCollisionRegistry::instance();
  auto &terrain_service = Game::Map::TerrainService::instance();
  Pathfinding *pathfinder = CommandService::get_pathfinder();

  if (registry.is_circle_overlapping_building(pos.x(), pos.z(), unit_radius,
                                              ignoreEntity)) {
    return false;
  }

  if (pathfinder != nullptr) {
    int const grid_x =
        static_cast<int>(std::round(pos.x() - pathfinder->get_grid_offset_x()));
    int const grid_z =
        static_cast<int>(std::round(pos.z() - pathfinder->get_grid_offset_z()));
    if (!pathfinder->is_walkable_with_radius(grid_x, grid_z, unit_radius)) {
      return false;
    }
  } else if (terrain_service.is_initialized()) {
    int const grid_x = static_cast<int>(std::round(pos.x()));
    int const grid_z = static_cast<int>(std::round(pos.z()));
    if (!terrain_service.is_walkable(grid_x, grid_z)) {
      return false;
    }
  }

  return true;
}

auto is_segment_walkable(const QVector3D &from, const QVector3D &to,
                         Engine::Core::EntityID ignoreEntity,
                         float unit_radius = 0.5F) -> bool {
  QVector3D const delta = to - from;
  float const distance_squared = delta.lengthSquared();

  bool const start_allowed = is_point_allowed(from, ignoreEntity, unit_radius);
  bool const end_allowed = is_point_allowed(to, ignoreEntity, unit_radius);

  if (distance_squared < 0.000001F) {
    return end_allowed;
  }

  float const distance = std::sqrt(distance_squared);
  int const steps = std::max(1, static_cast<int>(std::ceil(distance)) * 2);
  QVector3D const step = delta / static_cast<float>(steps);
  bool exited_blocked_zone = start_allowed;

  for (int i = 1; i <= steps; ++i) {
    QVector3D const pos = from + step * static_cast<float>(i);
    bool const allowed = is_point_allowed(pos, ignoreEntity, unit_radius);

    if (!exited_blocked_zone) {
      if (allowed) {
        exited_blocked_zone = true;
      }
      continue;
    }

    if (!allowed) {
      return false;
    }
  }

  return end_allowed && exited_blocked_zone;
}

// Detect and recover from deadlock situations
auto try_unstuck_unit(Engine::Core::Entity *entity, 
                      Engine::Core::TransformComponent *transform,
                      Engine::Core::MovementComponent *movement,
                      float unit_radius,
                      float delta_time) -> bool {
  // Check if unit is making progress
  float const dx = transform->position.x - movement->last_position_x;
  float const dz = transform->position.z - movement->last_position_z;
  float const distance_moved = std::sqrt(dx * dx + dz * dz);

  // Update stuck tracking
  if (distance_moved < kStuckDetectionThreshold && movement->has_target) {
    movement->time_stuck += delta_time;
  } else {
    movement->time_stuck = 0.0F;
  }

  // Update position tracking
  movement->last_position_x = transform->position.x;
  movement->last_position_z = transform->position.z;

  // Decrement unstuck cooldown
  if (movement->unstuck_cooldown > 0.0F) {
    movement->unstuck_cooldown -= delta_time;
  }

  // If stuck for too long and not in cooldown, apply recovery
  if (movement->time_stuck > kStuckTimeThreshold && 
      movement->unstuck_cooldown <= 0.0F &&
      movement->has_target) {
    
    Pathfinding *pathfinder = CommandService::get_pathfinder();
    if (pathfinder != nullptr) {
      // Find a nearby walkable position with a small random offset
      Point const current_grid = CommandService::world_to_grid(
          transform->position.x, transform->position.z);
      
      // Search for a nearby walkable point
      Point const nearest = Pathfinding::find_nearest_walkable_point(
          current_grid, kNearestPointSearchRadius, *pathfinder, unit_radius);
      
      if (!(nearest == current_grid)) {
        // Move to the nearest walkable point with a random offset to avoid clustering
        QVector3D safe_pos = CommandService::grid_to_world(nearest);
        
        // Add small random offset to prevent multiple units from clustering
        // Use thread_local to ensure thread safety
        thread_local std::random_device rd;
        thread_local std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-kUnstuckOffsetRadius, kUnstuckOffsetRadius);
        
        float const offset_x = dist(gen);
        float const offset_z = dist(gen);
        
        QVector3D const offset_pos(safe_pos.x() + offset_x, safe_pos.y(), 
                                   safe_pos.z() + offset_z);
        
        // Verify the offset position is still walkable
        if (is_point_allowed(offset_pos, entity->get_id(), unit_radius)) {
          safe_pos = offset_pos;
        }
        
        transform->position.x = safe_pos.x();
        transform->position.z = safe_pos.z();
        
        // Reset stuck state and set cooldown
        movement->time_stuck = 0.0F;
        movement->unstuck_cooldown = kUnstuckCooldown;
        
        // Clear path to force repath
        movement->clear_path();
        movement->has_target = false;
        movement->repath_cooldown = 0.0F;
        
        return true;
      }
    }
    
    // If we couldn't find a better position, just reset the path
    movement->time_stuck = 0.0F;
    movement->unstuck_cooldown = kUnstuckCooldown;
    movement->clear_path();
    movement->has_target = false;
    movement->repath_cooldown = 0.0F;
    
    return true;
  }

  return false;
}

} // namespace

void MovementSystem::update(Engine::Core::World *world, float delta_time) {
  CommandService::process_path_results(*world);
  auto entities = world->get_entities_with<Engine::Core::MovementComponent>();

  for (auto *entity : entities) {
    move_unit(entity, world, delta_time);
  }
}

void MovementSystem::move_unit(Engine::Core::Entity *entity,
                               Engine::Core::World *world, float delta_time) {
  auto *transform = entity->get_component<Engine::Core::TransformComponent>();
  auto *movement = entity->get_component<Engine::Core::MovementComponent>();
  auto *unit = entity->get_component<Engine::Core::UnitComponent>();

  if ((transform == nullptr) || (movement == nullptr) || (unit == nullptr)) {
    return;
  }

  if (unit->health <= 0 ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return;
  }

  auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  bool in_hold_mode = false;
  if (hold_mode != nullptr) {
    if (hold_mode->exit_cooldown > 0.0F) {
      hold_mode->exit_cooldown =
          std::max(0.0F, hold_mode->exit_cooldown - delta_time);
    }

    if (hold_mode->active) {
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
      in_hold_mode = true;
    }

    if (hold_mode->exit_cooldown > 0.0F && !in_hold_mode) {
      movement->vx = 0.0F;
      movement->vz = 0.0F;

      return;
    }
  }

  if (in_hold_mode) {
    if (!entity->has_component<Engine::Core::BuildingComponent>()) {
      if (transform->has_desired_yaw) {
        float const current = transform->rotation.y;
        float const target_yaw = transform->desired_yaw;
        float const diff =
            std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
        float const turn_speed = 180.0F;
        float const step =
            std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
        transform->rotation.y = current + step;

        if (std::fabs(diff) < 0.5F) {
          transform->has_desired_yaw = false;
        }
      }
    }
    return;
  }

  auto *atk = entity->get_component<Engine::Core::AttackComponent>();
  if ((atk != nullptr) && atk->in_melee_lock) {

    movement->has_target = false;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    movement->clear_path();
    movement->path_pending = false;
    return;
  }

  auto *builder_prod =
      entity->get_component<Engine::Core::BuilderProductionComponent>();
  bool const bypass_mode =
      (builder_prod != nullptr) && builder_prod->bypass_movement_active;

  if (bypass_mode) {

    float const dx = builder_prod->bypass_target_x - transform->position.x;
    float const dz = builder_prod->bypass_target_z - transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    constexpr float kBypassArrivalDistSq = 0.25F;

    if (dist_sq < kBypassArrivalDistSq) {

      transform->position.x = builder_prod->bypass_target_x;
      transform->position.z = builder_prod->bypass_target_z;
      builder_prod->bypass_movement_active = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->has_target = false;
      movement->clear_path();
    } else {

      float const dist = std::sqrt(std::max(dist_sq, 0.0001F));
      float const nx = dx / dist;
      float const nz = dz / dist;
      float base_speed = std::max(0.1F, unit->speed);
      movement->vx = nx * base_speed;
      movement->vz = nz * base_speed;

      transform->position.x += movement->vx * delta_time;
      transform->position.z += movement->vz * delta_time;

      float const target_yaw = std::atan2(movement->vx, movement->vz) * 180.0F /
                               std::numbers::pi_v<float>;
      float const current = transform->rotation.y;
      float const diff =
          std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = 720.0F;
      float const step =
          std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
      transform->rotation.y = current + step;
    }
    return;
  }

  QVector3D const final_goal(movement->goal_x, 0.0F, movement->goal_y);

  float const unit_radius =
      CommandService::get_unit_radius(*world, entity->get_id());

  QVector3D const current_pos_3d(transform->position.x, 0.0F,
                                 transform->position.z);
  bool const current_pos_valid =
      is_point_allowed(current_pos_3d, entity->get_id(), unit_radius);

  if (!current_pos_valid && !movement->path_pending) {
    Pathfinding *pathfinder = CommandService::get_pathfinder();
    if (pathfinder != nullptr) {
      Point const current_grid = CommandService::world_to_grid(
          transform->position.x, transform->position.z);
      Point const nearest = Pathfinding::find_nearest_walkable_point(
          current_grid, 10, *pathfinder, unit_radius);

      if (!(nearest == current_grid)) {
        QVector3D const safe_pos = CommandService::grid_to_world(nearest);
        transform->position.x = safe_pos.x();
        transform->position.z = safe_pos.z();
      }
    }
  }

  bool const destination_allowed =
      is_point_allowed(final_goal, entity->get_id(), unit_radius);

  if (movement->has_target && !destination_allowed) {
    movement->clear_path();
    movement->has_target = false;
    movement->path_pending = false;
    movement->pending_request_id = 0;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    return;
  }

  if (movement->repath_cooldown > 0.0F) {
    movement->repath_cooldown =
        std::max(0.0F, movement->repath_cooldown - delta_time);
  }

  if (movement->time_since_last_path_request < 10.0F) {
    movement->time_since_last_path_request += delta_time;
  }

  // Automatic deadlock detection and recovery
  if (try_unstuck_unit(entity, transform, movement, unit_radius, delta_time)) {
    // Unit was unstuck, let it repath naturally on next frame
    return;
  }

  float base_speed = std::max(0.1F, unit->speed);
  auto *stamina = entity->get_component<Engine::Core::StaminaComponent>();
  if (stamina != nullptr && stamina->is_running) {
    base_speed *= Engine::Core::StaminaComponent::kRunSpeedMultiplier;
  }
  const float max_speed = base_speed;
  const float accel = max_speed * 4.0F;
  const float damping = 6.0F;

  if (!movement->has_target) {
    QVector3D const current_pos(transform->position.x, 0.0F,
                                transform->position.z);
    float const goal_dist_sq = (final_goal - current_pos).lengthSquared();
    constexpr float k_stuck_distance_sq = 0.6F * 0.6F;

    bool requested_recovery_move = false;
    if (!movement->path_pending && movement->repath_cooldown <= 0.0F &&
        goal_dist_sq > k_stuck_distance_sq && std::isfinite(goal_dist_sq) &&
        destination_allowed) {
      CommandService::MoveOptions opts;
      opts.clear_attack_intent = false;
      opts.allow_direct_fallback = true;
      std::vector<Engine::Core::EntityID> const ids = {entity->get_id()};
      std::vector<QVector3D> const targets = {final_goal};
      CommandService::move_units(*world, ids, targets, opts);
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
      const auto &wp = movement->current_waypoint();
      segment_target = QVector3D(wp.first, 0.0F, wp.second);
    }
    auto refresh_segment_target = [&]() {
      if (movement->has_waypoints()) {
        const auto &wp = movement->current_waypoint();
        movement->target_x = wp.first;
        movement->target_y = wp.second;
        segment_target =
            QVector3D(movement->target_x, 0.0F, movement->target_y);
      } else {
        segment_target =
            QVector3D(movement->target_x, 0.0F, movement->target_y);
      }
    };

    auto try_advance_past_blocked_segment = [&]() {
      bool recovered = false;
      int skips_remaining = max_waypoint_skip_count;
      while (movement->has_waypoints() && skips_remaining-- > 0) {
        movement->advance_waypoint();
        refresh_segment_target();
        if (is_segment_walkable(current_pos, segment_target, entity->get_id(),
                                unit_radius)) {
          recovered = true;
          break;
        }
      }

      if (!recovered && !movement->has_waypoints()) {
        refresh_segment_target();
        if (is_segment_walkable(current_pos, segment_target, entity->get_id(),
                                unit_radius)) {
          recovered = true;
        }
      }

      return recovered;
    };

    if (!is_segment_walkable(current_pos, segment_target, entity->get_id(),
                             unit_radius)) {
      if (try_advance_past_blocked_segment()) {

      } else {
        bool issued_path_request = false;
        if (!movement->path_pending && movement->repath_cooldown <= 0.0F) {
          float const goal_dist_sq = (final_goal - current_pos).lengthSquared();
          if (goal_dist_sq > 0.01F && destination_allowed) {
            CommandService::MoveOptions opts;
            opts.clear_attack_intent = false;
            opts.allow_direct_fallback = false;
            std::vector<Engine::Core::EntityID> const ids = {entity->get_id()};
            std::vector<QVector3D> const targets = {
                QVector3D(movement->goal_x, 0.0F, movement->goal_y)};
            CommandService::move_units(*world, ids, targets, opts);
            movement->repath_cooldown = repath_cooldown_seconds;
            issued_path_request = true;
          }
        }

        if (!issued_path_request) {
          movement->path_pending = false;
          movement->pending_request_id = 0;
        }

        movement->clear_path();
        movement->has_target = false;
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        return;
      }
    }

    float const arrive_radius =
        std::clamp(max_speed * delta_time * 2.0F, 0.05F, 0.25F);
    float const arrive_radius_sq = arrive_radius * arrive_radius;

    float dx = movement->target_x - transform->position.x;
    float dz = movement->target_y - transform->position.z;
    float dist2 = dx * dx + dz * dz;

    int safety_counter = max_waypoint_skip_count;
    while (movement->has_target && dist2 < arrive_radius_sq &&
           safety_counter-- > 0) {
      if (movement->has_waypoints()) {
        movement->advance_waypoint();
        if (movement->has_waypoints()) {
          const auto &wp = movement->current_waypoint();
          movement->target_x = wp.first;
          movement->target_y = wp.second;
          dx = movement->target_x - transform->position.x;
          dz = movement->target_y - transform->position.z;
          dist2 = dx * dx + dz * dz;
          continue;
        }
      }

      QVector3D const target_pos(movement->target_x, 0.0F, movement->target_y);
      auto &registry = BuildingCollisionRegistry::instance();
      if (!registry.is_circle_overlapping_building(
              target_pos.x(), target_pos.z(), unit_radius, entity->get_id())) {

        transform->position.x = movement->target_x;
        transform->position.z = movement->target_y;
      } else {

        Pathfinding *pathfinder = CommandService::get_pathfinder();
        if (pathfinder != nullptr) {
          Point const target_grid =
              CommandService::world_to_grid(target_pos.x(), target_pos.z());
          Point const nearest = Pathfinding::find_nearest_walkable_point(
              target_grid, kNearestPointSearchRadius, *pathfinder, unit_radius);
          QVector3D const safe_pos = CommandService::grid_to_world(nearest);

          if (!registry.is_circle_overlapping_building(
                  safe_pos.x(), safe_pos.z(), unit_radius, entity->get_id())) {
            transform->position.x = safe_pos.x();
            transform->position.z = safe_pos.z();
          }
        }
      }
      movement->has_target = false;
      movement->vx = movement->vz = 0.0F;

      auto *guard_mode =
          entity->get_component<Engine::Core::GuardModeComponent>();
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

  float const old_x = transform->position.x;
  float const old_z = transform->position.z;

  transform->position.x += movement->vx * delta_time;
  transform->position.z += movement->vz * delta_time;

  {
    auto &registry = BuildingCollisionRegistry::instance();
    QVector3D const new_pos_3d(transform->position.x, 0.0F,
                               transform->position.z);

    if (registry.is_circle_overlapping_building(
            new_pos_3d.x(), new_pos_3d.z(), unit_radius, entity->get_id())) {

      Pathfinding *pathfinder = CommandService::get_pathfinder();
      if (pathfinder != nullptr) {
        Point const new_grid =
            CommandService::world_to_grid(new_pos_3d.x(), new_pos_3d.z());
        Point const nearest = Pathfinding::find_nearest_walkable_point(
            new_grid, kNearestPointSearchRadius, *pathfinder, unit_radius);

        QVector3D const safe_pos = CommandService::grid_to_world(nearest);

        if (!registry.is_circle_overlapping_building(
                safe_pos.x(), safe_pos.z(), unit_radius, entity->get_id())) {
          transform->position.x = safe_pos.x();
          transform->position.z = safe_pos.z();
        } else {

          transform->position.x = old_x;
          transform->position.z = old_z;
        }
      } else {

        transform->position.x = old_x;
        transform->position.z = old_z;
      }

      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
  }

  auto &terrain = Game::Map::TerrainService::instance();
  if (terrain.is_initialized()) {
    const Game::Map::TerrainHeightMap *hm = terrain.get_height_map();
    if (hm != nullptr) {
      const float tile = hm->getTileSize();
      const int w = hm->getWidth();
      const int h = hm->getHeight();
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

  if (!entity->has_component<Engine::Core::BuildingComponent>()) {
    float const speed2 =
        movement->vx * movement->vx + movement->vz * movement->vz;
    if (speed2 > 1e-5F) {
      float const target_yaw = std::atan2(movement->vx, movement->vz) * 180.0F /
                               std::numbers::pi_v<float>;

      float const current = transform->rotation.y;

      float const diff =
          std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = 720.0F;
      float const step =
          std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
      transform->rotation.y = current + step;
    } else if (transform->has_desired_yaw) {

      float const current = transform->rotation.y;
      float const target_yaw = transform->desired_yaw;
      float const diff =
          std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
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

auto MovementSystem::has_reached_target(
    const Engine::Core::TransformComponent *transform,
    const Engine::Core::MovementComponent *movement) -> bool {
  float const dx = movement->target_x - transform->position.x;
  float const dz = movement->target_y - transform->position.z;
  float const distance_squared = dx * dx + dz * dz;

  const float threshold = 0.1F;
  return distance_squared < threshold * threshold;
}

} // namespace Game::Systems