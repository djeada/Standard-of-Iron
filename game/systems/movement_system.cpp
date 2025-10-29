#include "movement_system.h"
#include "../map/terrain_service.h"
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
#include <vector>

namespace Game::Systems {

static constexpr int max_waypoint_skip_count = 4;
static constexpr float repath_cooldown_seconds = 0.4F;

namespace {

auto isPointAllowed(const QVector3D &pos,
                    Engine::Core::EntityID ignoreEntity) -> bool {
  auto &registry = BuildingCollisionRegistry::instance();
  auto &terrain_service = Game::Map::TerrainService::instance();
  Pathfinding *pathfinder = CommandService::getPathfinder();

  if (registry.isPointInBuilding(pos.x(), pos.z(), ignoreEntity)) {
    return false;
  }

  if (pathfinder != nullptr) {
    int const grid_x =
        static_cast<int>(std::round(pos.x() - pathfinder->getGridOffsetX()));
    int const grid_z =
        static_cast<int>(std::round(pos.z() - pathfinder->getGridOffsetZ()));
    if (!pathfinder->isWalkable(grid_x, grid_z)) {
      return false;
    }
  } else if (terrain_service.isInitialized()) {
    int const grid_x = static_cast<int>(std::round(pos.x()));
    int const grid_z = static_cast<int>(std::round(pos.z()));
    if (!terrain_service.isWalkable(grid_x, grid_z)) {
      return false;
    }
  }

  return true;
}

auto isSegmentWalkable(const QVector3D &from, const QVector3D &to,
                       Engine::Core::EntityID ignoreEntity) -> bool {
  QVector3D const delta = to - from;
  float const distance_squared = delta.lengthSquared();

  bool const start_allowed = isPointAllowed(from, ignoreEntity);
  bool const end_allowed = isPointAllowed(to, ignoreEntity);

  if (distance_squared < 0.000001F) {
    return end_allowed;
  }

  float const distance = std::sqrt(distance_squared);
  int const steps = std::max(1, static_cast<int>(std::ceil(distance)) * 2);
  QVector3D const step = delta / static_cast<float>(steps);
  bool exited_blocked_zone = start_allowed;

  for (int i = 1; i <= steps; ++i) {
    QVector3D const pos = from + step * static_cast<float>(i);
    bool const allowed = isPointAllowed(pos, ignoreEntity);

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

} // namespace

void MovementSystem::update(Engine::Core::World *world, float deltaTime) {
  CommandService::processPathResults(*world);
  auto entities = world->getEntitiesWith<Engine::Core::MovementComponent>();

  for (auto *entity : entities) {
    moveUnit(entity, world, deltaTime);
  }
}

void MovementSystem::moveUnit(Engine::Core::Entity *entity,
                              Engine::Core::World *world, float deltaTime) {
  auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
  auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
  auto *unit = entity->getComponent<Engine::Core::UnitComponent>();

  if ((transform == nullptr) || (movement == nullptr) || (unit == nullptr)) {
    return;
  }

  if (unit->health <= 0 ||
      entity->hasComponent<Engine::Core::PendingRemovalComponent>()) {
    return;
  }

  auto *hold_mode = entity->getComponent<Engine::Core::HoldModeComponent>();
  bool in_hold_mode = false;
  if (hold_mode != nullptr) {
    if (hold_mode->exitCooldown > 0.0F) {
      hold_mode->exitCooldown =
          std::max(0.0F, hold_mode->exitCooldown - deltaTime);
    }

    if (hold_mode->active) {
      movement->hasTarget = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->path.clear();
      movement->pathPending = false;
      in_hold_mode = true;
    }

    if (hold_mode->exitCooldown > 0.0F && !in_hold_mode) {
      movement->vx = 0.0F;
      movement->vz = 0.0F;

      return;
    }
  }

  if (in_hold_mode) {
    if (!entity->hasComponent<Engine::Core::BuildingComponent>()) {
      if (transform->hasDesiredYaw) {
        float const current = transform->rotation.y;
        float const target_yaw = transform->desiredYaw;
        float const diff =
            std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
        float const turn_speed = 180.0F;
        float const step =
            std::clamp(diff, -turn_speed * deltaTime, turn_speed * deltaTime);
        transform->rotation.y = current + step;

        if (std::fabs(diff) < 0.5F) {
          transform->hasDesiredYaw = false;
        }
      }
    }
    return;
  }

  auto *atk = entity->getComponent<Engine::Core::AttackComponent>();
  if ((atk != nullptr) && atk->inMeleeLock) {

    movement->hasTarget = false;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    movement->path.clear();
    movement->pathPending = false;
    return;
  }

  QVector3D const final_goal(movement->goalX, 0.0F, movement->goalY);
  bool const destination_allowed = isPointAllowed(final_goal, entity->getId());

  if (movement->hasTarget && !destination_allowed) {
    movement->path.clear();
    movement->hasTarget = false;
    movement->pathPending = false;
    movement->pendingRequestId = 0;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    return;
  }

  if (movement->repathCooldown > 0.0F) {
    movement->repathCooldown =
        std::max(0.0F, movement->repathCooldown - deltaTime);
  }

  if (movement->timeSinceLastPathRequest < 10.0F) {
    movement->timeSinceLastPathRequest += deltaTime;
  }

  const float max_speed = std::max(0.1F, unit->speed);
  const float accel = max_speed * 4.0F;
  const float damping = 6.0F;

  if (!movement->hasTarget) {
    QVector3D const current_pos(transform->position.x, 0.0F,
                                transform->position.z);
    float const goal_dist_sq = (final_goal - current_pos).lengthSquared();
    constexpr float k_stuck_distance_sq = 0.6F * 0.6F;

    bool requested_recovery_move = false;
    if (!movement->pathPending && movement->repathCooldown <= 0.0F &&
        goal_dist_sq > k_stuck_distance_sq && std::isfinite(goal_dist_sq) &&
        destination_allowed) {
      CommandService::MoveOptions opts;
      opts.clearAttackIntent = false;
      opts.allowDirectFallback = true;
      std::vector<Engine::Core::EntityID> const ids = {entity->getId()};
      std::vector<QVector3D> const targets = {final_goal};
      CommandService::moveUnits(*world, ids, targets, opts);
      movement->repathCooldown = repath_cooldown_seconds;
      requested_recovery_move = true;
    }

    if (!requested_recovery_move) {
      movement->vx *= std::max(0.0F, 1.0F - damping * deltaTime);
      movement->vz *= std::max(0.0F, 1.0F - damping * deltaTime);
    }
  } else {
    QVector3D current_pos(transform->position.x, 0.0F, transform->position.z);
    QVector3D segment_target(movement->target_x, 0.0F, movement->target_y);
    if (!movement->path.empty()) {
      segment_target = QVector3D(movement->path.front().first, 0.0F,
                                 movement->path.front().second);
    }
    auto refresh_segment_target = [&]() {
      if (!movement->path.empty()) {
        movement->target_x = movement->path.front().first;
        movement->target_y = movement->path.front().second;
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
      while (!movement->path.empty() && skips_remaining-- > 0) {
        movement->path.erase(movement->path.begin());
        refresh_segment_target();
        if (isSegmentWalkable(current_pos, segment_target, entity->getId())) {
          recovered = true;
          break;
        }
      }

      if (!recovered && movement->path.empty()) {
        refresh_segment_target();
        if (isSegmentWalkable(current_pos, segment_target, entity->getId())) {
          recovered = true;
        }
      }

      return recovered;
    };

    if (!isSegmentWalkable(current_pos, segment_target, entity->getId())) {
      if (try_advance_past_blocked_segment()) {

      } else {
        bool issued_path_request = false;
        if (!movement->pathPending && movement->repathCooldown <= 0.0F) {
          float const goal_dist_sq = (final_goal - current_pos).lengthSquared();
          if (goal_dist_sq > 0.01F && destination_allowed) {
            CommandService::MoveOptions opts;
            opts.clearAttackIntent = false;
            opts.allowDirectFallback = false;
            std::vector<Engine::Core::EntityID> const ids = {entity->getId()};
            std::vector<QVector3D> const targets = {
                QVector3D(movement->goalX, 0.0F, movement->goalY)};
            CommandService::moveUnits(*world, ids, targets, opts);
            movement->repathCooldown = repath_cooldown_seconds;
            issued_path_request = true;
          }
        }

        if (!issued_path_request) {
          movement->pathPending = false;
          movement->pendingRequestId = 0;
        }

        movement->path.clear();
        movement->hasTarget = false;
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        return;
      }
    }

    float const arrive_radius =
        std::clamp(max_speed * deltaTime * 2.0F, 0.05F, 0.25F);
    float const arrive_radius_sq = arrive_radius * arrive_radius;

    float dx = movement->target_x - transform->position.x;
    float dz = movement->target_y - transform->position.z;
    float dist2 = dx * dx + dz * dz;

    int safety_counter = max_waypoint_skip_count;
    while (movement->hasTarget && dist2 < arrive_radius_sq &&
           safety_counter-- > 0) {
      if (!movement->path.empty()) {
        movement->path.erase(movement->path.begin());
        if (!movement->path.empty()) {
          movement->target_x = movement->path.front().first;
          movement->target_y = movement->path.front().second;
          dx = movement->target_x - transform->position.x;
          dz = movement->target_y - transform->position.z;
          dist2 = dx * dx + dz * dz;
          continue;
        }
      }

      transform->position.x = movement->target_x;
      transform->position.z = movement->target_y;
      movement->hasTarget = false;
      movement->vx = movement->vz = 0.0F;
      break;
    }

    if (!movement->hasTarget) {
      movement->vx *= std::max(0.0F, 1.0F - damping * deltaTime);
      movement->vz *= std::max(0.0F, 1.0F - damping * deltaTime);
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
      movement->vx += ax * deltaTime;
      movement->vz += az * deltaTime;

      movement->vx *= std::max(0.0F, 1.0F - 0.5F * damping * deltaTime);
      movement->vz *= std::max(0.0F, 1.0F - 0.5F * damping * deltaTime);
    }
  }

  transform->position.x += movement->vx * deltaTime;
  transform->position.z += movement->vz * deltaTime;

  auto &terrain = Game::Map::TerrainService::instance();
  if (terrain.isInitialized()) {
    const Game::Map::TerrainHeightMap *hm = terrain.getHeightMap();
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

  if (!entity->hasComponent<Engine::Core::BuildingComponent>()) {
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
          std::clamp(diff, -turn_speed * deltaTime, turn_speed * deltaTime);
      transform->rotation.y = current + step;
    } else if (transform->hasDesiredYaw) {

      float const current = transform->rotation.y;
      float const target_yaw = transform->desiredYaw;
      float const diff =
          std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = 180.0F;
      float const step =
          std::clamp(diff, -turn_speed * deltaTime, turn_speed * deltaTime);
      transform->rotation.y = current + step;

      if (std::fabs(diff) < 0.5F) {
        transform->hasDesiredYaw = false;
      }
    }
  }
}

auto MovementSystem::hasReachedTarget(
    const Engine::Core::TransformComponent *transform,
    const Engine::Core::MovementComponent *movement) -> bool {
  float const dx = movement->target_x - transform->position.x;
  float const dz = movement->target_y - transform->position.z;
  float const distance_squared = dx * dx + dz * dz;

  const float threshold = 0.1F;
  return distance_squared < threshold * threshold;
}

} // namespace Game::Systems