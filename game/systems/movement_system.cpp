#include "movement_system.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "pathfinding.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Game::Systems {

static constexpr int MAX_WAYPOINT_SKIP_COUNT = 4;
static constexpr float REPATH_COOLDOWN_SECONDS = 0.4f;

namespace {

bool isSegmentWalkable(const QVector3D &from, const QVector3D &to,
                       Engine::Core::EntityID ignoreEntity) {
  auto &registry = BuildingCollisionRegistry::instance();
  auto &terrainService = Game::Map::TerrainService::instance();
  Pathfinding *pathfinder = CommandService::getPathfinder();

  auto samplePoint = [&](const QVector3D &pos) {
    if (registry.isPointInBuilding(pos.x(), pos.z(), ignoreEntity)) {
      return false;
    }

    if (pathfinder) {
      int gridX =
          static_cast<int>(std::round(pos.x() - pathfinder->getGridOffsetX()));
      int gridZ =
          static_cast<int>(std::round(pos.z() - pathfinder->getGridOffsetZ()));
      if (!pathfinder->isWalkable(gridX, gridZ)) {
        return false;
      }
    } else if (terrainService.isInitialized()) {
      int gridX = static_cast<int>(std::round(pos.x()));
      int gridZ = static_cast<int>(std::round(pos.z()));
      if (!terrainService.isWalkable(gridX, gridZ)) {
        return false;
      }
    }

    return true;
  };

  QVector3D delta = to - from;
  float distance = delta.length();

  if (distance < 0.001f) {
    return samplePoint(from);
  }

  int steps = std::max(1, static_cast<int>(std::ceil(distance)) * 2);
  QVector3D step = delta / static_cast<float>(steps);
  QVector3D pos = from;
  for (int i = 0; i <= steps; ++i, pos += step) {
    if (!samplePoint(pos)) {
      return false;
    }
  }

  return true;
}

} // namespace

void MovementSystem::update(Engine::Core::World *world, float deltaTime) {
  CommandService::processPathResults(*world);
  auto entities = world->getEntitiesWith<Engine::Core::MovementComponent>();

  for (auto entity : entities) {
    moveUnit(entity, world, deltaTime);
  }
}

void MovementSystem::moveUnit(Engine::Core::Entity *entity,
                              Engine::Core::World *world, float deltaTime) {
  auto transform = entity->getComponent<Engine::Core::TransformComponent>();
  auto movement = entity->getComponent<Engine::Core::MovementComponent>();
  auto unit = entity->getComponent<Engine::Core::UnitComponent>();

  if (!transform || !movement || !unit) {
    return;
  }

  if (movement->repathCooldown > 0.0f) {
    movement->repathCooldown =
        std::max(0.0f, movement->repathCooldown - deltaTime);
  }

  const float maxSpeed = std::max(0.1f, unit->speed);
  const float accel = maxSpeed * 4.0f;
  const float damping = 6.0f;

  if (!movement->hasTarget) {
    movement->vx *= std::max(0.0f, 1.0f - damping * deltaTime);
    movement->vz *= std::max(0.0f, 1.0f - damping * deltaTime);
  } else {
    QVector3D currentPos(transform->position.x, 0.0f, transform->position.z);
    QVector3D segmentTarget(movement->targetX, 0.0f, movement->targetY);
    if (!movement->path.empty()) {
      segmentTarget = QVector3D(movement->path.front().first, 0.0f,
                                movement->path.front().second);
    }
    QVector3D finalGoal(movement->goalX, 0.0f, movement->goalY);

    if (!isSegmentWalkable(currentPos, segmentTarget, entity->getId())) {
      bool issuedPathRequest = false;
      if (!movement->pathPending && movement->repathCooldown <= 0.0f) {
        float goalDistSq = (finalGoal - currentPos).lengthSquared();
        if (goalDistSq > 0.01f) {
          CommandService::MoveOptions opts;
          opts.clearAttackIntent = false;
          opts.allowDirectFallback = false;
          std::vector<Engine::Core::EntityID> ids = {entity->getId()};
          std::vector<QVector3D> targets = {
              QVector3D(movement->goalX, 0.0f, movement->goalY)};
          CommandService::moveUnits(*world, ids, targets, opts);
          movement->repathCooldown = REPATH_COOLDOWN_SECONDS;
          issuedPathRequest = true;
        }
      }

      if (!issuedPathRequest) {
        movement->pathPending = false;
        movement->pendingRequestId = 0;
      }

      movement->path.clear();
      movement->hasTarget = false;
      movement->vx = 0.0f;
      movement->vz = 0.0f;
      return;
    }

    float arriveRadius = std::clamp(maxSpeed * deltaTime * 2.0f, 0.05f, 0.25f);
    float arriveRadiusSq = arriveRadius * arriveRadius;

    float dx = movement->targetX - transform->position.x;
    float dz = movement->targetY - transform->position.z;
    float dist2 = dx * dx + dz * dz;

    int safetyCounter = MAX_WAYPOINT_SKIP_COUNT;
    while (movement->hasTarget && dist2 < arriveRadiusSq &&
           safetyCounter-- > 0) {
      if (!movement->path.empty()) {
        movement->path.erase(movement->path.begin());
        if (!movement->path.empty()) {
          movement->targetX = movement->path.front().first;
          movement->targetY = movement->path.front().second;
          dx = movement->targetX - transform->position.x;
          dz = movement->targetY - transform->position.z;
          dist2 = dx * dx + dz * dz;
          continue;
        }
      }

      transform->position.x = movement->targetX;
      transform->position.z = movement->targetY;
      movement->hasTarget = false;
      movement->vx = movement->vz = 0.0f;
      break;
    }

    if (!movement->hasTarget) {
      movement->vx *= std::max(0.0f, 1.0f - damping * deltaTime);
      movement->vz *= std::max(0.0f, 1.0f - damping * deltaTime);
    } else {
      float distance = std::sqrt(std::max(dist2, 0.0f));
      float nx = dx / std::max(0.0001f, distance);
      float nz = dz / std::max(0.0001f, distance);
      float desiredSpeed = maxSpeed;
      float slowRadius = arriveRadius * 4.0f;
      if (distance < slowRadius) {
        desiredSpeed = maxSpeed * (distance / slowRadius);
      }
      float desiredVx = nx * desiredSpeed;
      float desiredVz = nz * desiredSpeed;

      float ax = (desiredVx - movement->vx) * accel;
      float az = (desiredVz - movement->vz) * accel;
      movement->vx += ax * deltaTime;
      movement->vz += az * deltaTime;

      movement->vx *= std::max(0.0f, 1.0f - 0.5f * damping * deltaTime);
      movement->vz *= std::max(0.0f, 1.0f - 0.5f * damping * deltaTime);
    }
  }

  transform->position.x += movement->vx * deltaTime;
  transform->position.z += movement->vz * deltaTime;

  auto &terrain = Game::Map::TerrainService::instance();
  if (terrain.isInitialized()) {
    const Game::Map::TerrainHeightMap *hm = terrain.getHeightMap();
    if (hm) {
      const float tile = hm->getTileSize();
      const int w = hm->getWidth();
      const int h = hm->getHeight();
      if (w > 0 && h > 0) {
        const float halfW = w * 0.5f - 0.5f;
        const float halfH = h * 0.5f - 0.5f;
        const float minX = -halfW * tile;
        const float maxX = halfW * tile;
        const float minZ = -halfH * tile;
        const float maxZ = halfH * tile;
        transform->position.x = std::clamp(transform->position.x, minX, maxX);
        transform->position.z = std::clamp(transform->position.z, minZ, maxZ);
      }
    }
  }

  if (!entity->hasComponent<Engine::Core::BuildingComponent>()) {
    float speed2 = movement->vx * movement->vx + movement->vz * movement->vz;
    if (speed2 > 1e-5f) {
      float targetYaw =
          std::atan2(movement->vx, movement->vz) * 180.0f / 3.14159265f;

      float current = transform->rotation.y;

      float diff = std::fmod((targetYaw - current + 540.0f), 360.0f) - 180.0f;
      float turnSpeed = 720.0f;
      float step =
          std::clamp(diff, -turnSpeed * deltaTime, turnSpeed * deltaTime);
      transform->rotation.y = current + step;
    } else if (transform->hasDesiredYaw) {

      float current = transform->rotation.y;
      float targetYaw = transform->desiredYaw;
      float diff = std::fmod((targetYaw - current + 540.0f), 360.0f) - 180.0f;
      float turnSpeed = 180.0f;
      float step =
          std::clamp(diff, -turnSpeed * deltaTime, turnSpeed * deltaTime);
      transform->rotation.y = current + step;

      if (std::fabs(diff) < 0.5f) {
        transform->hasDesiredYaw = false;
      }
    }
  }
}

bool MovementSystem::hasReachedTarget(
    const Engine::Core::TransformComponent *transform,
    const Engine::Core::MovementComponent *movement) {
  float dx = movement->targetX - transform->position.x;
  float dz = movement->targetY - transform->position.z;
  float distanceSquared = dx * dx + dz * dz;

  const float threshold = 0.1f;
  return distanceSquared < threshold * threshold;
}

} // namespace Game::Systems