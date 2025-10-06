#include "command_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include "building_collision_registry.h"
#include "pathfinding.h"
#include <QVector3D>
#include <cmath>
#include <cstdlib>

namespace Game {
namespace Systems {

std::unique_ptr<Pathfinding> CommandService::s_pathfinder = nullptr;
std::unordered_map<std::uint64_t, CommandService::PendingPathRequest>
    CommandService::s_pendingRequests;
std::unordered_map<Engine::Core::EntityID, std::uint64_t>
    CommandService::s_entityToRequest;
std::mutex CommandService::s_pendingMutex;
std::atomic<std::uint64_t> CommandService::s_nextRequestId{1};

void CommandService::initialize(int worldWidth, int worldHeight) {
  s_pathfinder = std::make_unique<Pathfinding>(worldWidth, worldHeight);
  {
    std::lock_guard<std::mutex> lock(s_pendingMutex);
    s_pendingRequests.clear();
    s_entityToRequest.clear();
  }
  s_nextRequestId.store(1, std::memory_order_release);

  float offsetX = -worldWidth / 2.0f;
  float offsetZ = -worldHeight / 2.0f;
  s_pathfinder->setGridOffset(offsetX, offsetZ);
}

Pathfinding *CommandService::getPathfinder() { return s_pathfinder.get(); }

Point CommandService::worldToGrid(float worldX, float worldZ) {

  if (s_pathfinder) {

    int gridX =
        static_cast<int>(std::round(worldX - s_pathfinder->getGridOffsetX()));
    int gridZ =
        static_cast<int>(std::round(worldZ - s_pathfinder->getGridOffsetZ()));
    return Point(gridX, gridZ);
  }

  return Point(static_cast<int>(std::round(worldX)),
               static_cast<int>(std::round(worldZ)));
}

QVector3D CommandService::gridToWorld(const Point &gridPos) {
  if (s_pathfinder) {
    return QVector3D(
        static_cast<float>(gridPos.x) + s_pathfinder->getGridOffsetX(), 0.0f,
        static_cast<float>(gridPos.y) + s_pathfinder->getGridOffsetZ());
  }
  return QVector3D(static_cast<float>(gridPos.x), 0.0f,
                   static_cast<float>(gridPos.y));
}

void CommandService::clearPendingRequest(Engine::Core::EntityID entityId) {
  std::lock_guard<std::mutex> lock(s_pendingMutex);
  auto it = s_entityToRequest.find(entityId);
  if (it != s_entityToRequest.end()) {
    s_pendingRequests.erase(it->second);
    s_entityToRequest.erase(it);
  }
}

void CommandService::moveUnits(Engine::Core::World &world,
                               const std::vector<Engine::Core::EntityID> &units,
                               const std::vector<QVector3D> &targets) {
  moveUnits(world, units, targets, MoveOptions{});
}

void CommandService::moveUnits(Engine::Core::World &world,
                               const std::vector<Engine::Core::EntityID> &units,
                               const std::vector<QVector3D> &targets,
                               const MoveOptions &options) {
  if (units.size() != targets.size())
    return;

  for (size_t i = 0; i < units.size(); ++i) {
    auto *e = world.getEntity(units[i]);
    if (!e)
      continue;

    auto *transform = e->getComponent<Engine::Core::TransformComponent>();
    if (!transform)
      continue;

    auto *mv = e->getComponent<Engine::Core::MovementComponent>();
    if (!mv)
      mv = e->addComponent<Engine::Core::MovementComponent>();
    if (!mv)
      continue;

    if (options.clearAttackIntent) {
      e->removeComponent<Engine::Core::AttackTargetComponent>();
    }

    if (s_pathfinder) {
      Point start = worldToGrid(transform->position.x, transform->position.z);
      Point end = worldToGrid(targets[i].x(), targets[i].z());

      int dx = std::abs(end.x - start.x);
      int dz = std::abs(end.y - start.y);
      bool useDirectPath = (dx + dz) <= CommandService::DIRECT_PATH_THRESHOLD;
      if (!options.allowDirectFallback) {
        useDirectPath = false;
      }

      if (useDirectPath) {

        mv->targetX = targets[i].x();
        mv->targetY = targets[i].z();
        mv->hasTarget = true;
        mv->path.clear();
        mv->pathPending = false;
        mv->pendingRequestId = 0;
        clearPendingRequest(units[i]);
      } else {

        mv->path.clear();
        mv->hasTarget = false;
        mv->pathPending = true;

        std::uint64_t requestId =
            s_nextRequestId.fetch_add(1, std::memory_order_relaxed);
        mv->pendingRequestId = requestId;

        {
          std::lock_guard<std::mutex> lock(s_pendingMutex);
          auto it = s_entityToRequest.find(units[i]);
          if (it != s_entityToRequest.end()) {
            s_pendingRequests.erase(it->second);
            s_entityToRequest.erase(it);
          }
          s_pendingRequests[requestId] = {units[i], targets[i], options};
          s_entityToRequest[units[i]] = requestId;
        }

        s_pathfinder->submitPathRequest(requestId, start, end);
      }
    } else {

      mv->targetX = targets[i].x();
      mv->targetY = targets[i].z();
      mv->hasTarget = true;
      mv->path.clear();
      mv->pathPending = false;
      mv->pendingRequestId = 0;
      clearPendingRequest(units[i]);
    }
  }
}

void CommandService::processPathResults(Engine::Core::World &world) {
  if (!s_pathfinder)
    return;

  auto results = s_pathfinder->fetchCompletedPaths();
  if (results.empty())
    return;

  for (auto &result : results) {
    PendingPathRequest requestInfo;
    bool found = false;

    {
      std::lock_guard<std::mutex> lock(s_pendingMutex);
      auto pendingIt = s_pendingRequests.find(result.requestId);
      if (pendingIt != s_pendingRequests.end()) {
        requestInfo = pendingIt->second;
        s_pendingRequests.erase(pendingIt);

        auto entityIt = s_entityToRequest.find(requestInfo.entityId);
        if (entityIt != s_entityToRequest.end() &&
            entityIt->second == result.requestId) {
          s_entityToRequest.erase(entityIt);
        }

        found = true;
      }
    }

    if (!found)
      continue;

    auto *entity = world.getEntity(requestInfo.entityId);
    if (!entity)
      continue;

    auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
    if (!movement)
      continue;

    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (!transform)
      continue;

    if (!movement->pathPending ||
        movement->pendingRequestId != result.requestId) {
      continue;
    }

    movement->pathPending = false;
    movement->pendingRequestId = 0;
    movement->path.clear();

    const auto &pathPoints = result.path;

    if (pathPoints.size() <= 1) {
      if (requestInfo.options.allowDirectFallback) {
        movement->targetX = requestInfo.target.x();
        movement->targetY = requestInfo.target.z();
        movement->hasTarget = true;
      } else {
        movement->hasTarget = false;
        movement->vx = 0.0f;
        movement->vz = 0.0f;
      }
      continue;
    }

    for (size_t idx = 1; idx < pathPoints.size(); ++idx) {
      const auto &point = pathPoints[idx];
      QVector3D worldPos = gridToWorld(point);
      movement->path.push_back({worldPos.x(), worldPos.z()});
    }

    const float skipThresholdSq = CommandService::WAYPOINT_SKIP_THRESHOLD_SQ;
    while (!movement->path.empty()) {
      float dx = movement->path.front().first - transform->position.x;
      float dz = movement->path.front().second - transform->position.z;
      if (dx * dx + dz * dz <= skipThresholdSq) {
        movement->path.erase(movement->path.begin());
      } else {
        break;
      }
    }

    if (!movement->path.empty()) {
      movement->targetX = movement->path[0].first;
      movement->targetY = movement->path[0].second;
      movement->hasTarget = true;
    } else {
      if (requestInfo.options.allowDirectFallback) {
        movement->targetX = requestInfo.target.x();
        movement->targetY = requestInfo.target.z();
        movement->hasTarget = true;
      } else {
        movement->hasTarget = false;
        movement->vx = 0.0f;
        movement->vz = 0.0f;
      }
    }
  }
}

void CommandService::attackTarget(
    Engine::Core::World &world,
    const std::vector<Engine::Core::EntityID> &units,
    Engine::Core::EntityID targetId, bool shouldChase) {
  if (targetId == 0)
    return;

  for (auto unitId : units) {
    auto *e = world.getEntity(unitId);
    if (!e)
      continue;

    auto *attackTarget = e->getComponent<Engine::Core::AttackTargetComponent>();
    if (!attackTarget) {
      attackTarget = e->addComponent<Engine::Core::AttackTargetComponent>();
    }
    if (!attackTarget)
      continue;

    attackTarget->targetId = targetId;
    attackTarget->shouldChase = shouldChase;
  }
}

} // namespace Systems
} // namespace Game
