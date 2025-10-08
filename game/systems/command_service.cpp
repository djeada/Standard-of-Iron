#include "command_service.h"
#include "../core/component.h"
#include "../core/world.h"
#include "pathfinding.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Game {
namespace Systems {

namespace {
constexpr float SAME_TARGET_THRESHOLD_SQ = 0.01f;
}

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

  float offsetX = -(worldWidth * 0.5f - 0.5f);
  float offsetZ = -(worldHeight * 0.5f - 0.5f);
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

    const float targetX = targets[i].x();
    const float targetZ = targets[i].z();

    bool matchedPending = false;
    if (mv->pathPending) {
      std::lock_guard<std::mutex> lock(s_pendingMutex);
      auto requestIt = s_entityToRequest.find(units[i]);
      if (requestIt != s_entityToRequest.end()) {
        auto pendingIt = s_pendingRequests.find(requestIt->second);
        if (pendingIt != s_pendingRequests.end()) {
          float pdx = pendingIt->second.target.x() - targetX;
          float pdz = pendingIt->second.target.z() - targetZ;
          if (pdx * pdx + pdz * pdz <= SAME_TARGET_THRESHOLD_SQ) {
            pendingIt->second.options = options;
            matchedPending = true;
          }
        } else {
          s_entityToRequest.erase(requestIt);
        }
      }
    }

    mv->goalX = targetX;
    mv->goalY = targetZ;

    if (matchedPending) {
      continue;
    }

    if (!mv->pathPending) {
      bool currentTargetMatches = mv->hasTarget && mv->path.empty();
      if (currentTargetMatches) {
        float dx = mv->targetX - targetX;
        float dz = mv->targetY - targetZ;
        if (dx * dx + dz * dz <= SAME_TARGET_THRESHOLD_SQ) {
          continue;
        }
      }

      if (!mv->path.empty()) {
        const auto &lastWaypoint = mv->path.back();
        float dx = lastWaypoint.first - targetX;
        float dz = lastWaypoint.second - targetZ;
        if (dx * dx + dz * dz <= SAME_TARGET_THRESHOLD_SQ) {
          continue;
        }
      }
    }

    if (s_pathfinder) {
      Point start = worldToGrid(transform->position.x, transform->position.z);
      Point end = worldToGrid(targets[i].x(), targets[i].z());

      if (start == end) {
        mv->targetX = targetX;
        mv->targetY = targetZ;
        mv->hasTarget = true;
        mv->path.clear();
        mv->pathPending = false;
        mv->pendingRequestId = 0;
        clearPendingRequest(units[i]);
        continue;
      }

      int dx = std::abs(end.x - start.x);
      int dz = std::abs(end.y - start.y);
      bool useDirectPath = (dx + dz) <= CommandService::DIRECT_PATH_THRESHOLD;
      if (!options.allowDirectFallback) {
        useDirectPath = false;
      }

      if (useDirectPath) {

        mv->targetX = targetX;
        mv->targetY = targetZ;
        mv->hasTarget = true;
        mv->path.clear();
        mv->pathPending = false;
        mv->pendingRequestId = 0;
        clearPendingRequest(units[i]);
      } else {

        bool skipNewRequest = false;
        {
          std::lock_guard<std::mutex> lock(s_pendingMutex);
          auto existingIt = s_entityToRequest.find(units[i]);
          if (existingIt != s_entityToRequest.end()) {
            auto pendingIt = s_pendingRequests.find(existingIt->second);
            if (pendingIt != s_pendingRequests.end()) {
              float dx = pendingIt->second.target.x() - targetX;
              float dz = pendingIt->second.target.z() - targetZ;
              if (dx * dx + dz * dz <= SAME_TARGET_THRESHOLD_SQ) {
                pendingIt->second.options = options;
                skipNewRequest = true;
              } else {
                s_pendingRequests.erase(pendingIt);
                s_entityToRequest.erase(existingIt);
              }
            } else {
              s_entityToRequest.erase(existingIt);
            }
          }
        }

        if (skipNewRequest) {
          continue;
        }

        mv->path.clear();
        mv->hasTarget = false;
        mv->pathPending = true;

        std::uint64_t requestId =
            s_nextRequestId.fetch_add(1, std::memory_order_relaxed);
        mv->pendingRequestId = requestId;

        {
          std::lock_guard<std::mutex> lock(s_pendingMutex);
          s_pendingRequests[requestId] = {units[i], targets[i], options};
          s_entityToRequest[units[i]] = requestId;
        }

        s_pathfinder->submitPathRequest(requestId, start, end);
      }
    } else {

      mv->targetX = targetX;
      mv->targetY = targetZ;
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

    if (!found) {
      continue;
    }

    auto *entity = world.getEntity(requestInfo.entityId);
    if (!entity)
      continue;

    auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
    if (!movement) {
      continue;
    }

    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (!transform) {
      continue;
    }

    if (!movement->pathPending ||
        movement->pendingRequestId != result.requestId) {
      continue;
    }

    movement->pathPending = false;
    movement->pendingRequestId = 0;
    movement->path.clear();
    movement->goalX = requestInfo.target.x();
    movement->goalY = requestInfo.target.z();

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

    movement->path.reserve(pathPoints.size() > 1 ? pathPoints.size() - 1 : 0);
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

    if (!shouldChase)
      continue;

    auto *targetEnt = world.getEntity(targetId);
    if (!targetEnt)
      continue;

    auto *tTrans = targetEnt->getComponent<Engine::Core::TransformComponent>();
    auto *attTrans = e->getComponent<Engine::Core::TransformComponent>();
    if (!tTrans || !attTrans)
      continue;

    QVector3D targetPos(tTrans->position.x, 0.0f, tTrans->position.z);
    QVector3D attackerPos(attTrans->position.x, 0.0f, attTrans->position.z);

    QVector3D desiredPos = targetPos;

    float range = 2.0f;
    if (auto *atk = e->getComponent<Engine::Core::AttackComponent>()) {
      range = std::max(0.1f, atk->range);
    }

    QVector3D direction = targetPos - attackerPos;
    float distance = direction.length();
    if (distance > 0.001f) {
      direction /= distance;
      if (targetEnt->hasComponent<Engine::Core::BuildingComponent>()) {
        float scaleX = tTrans->scale.x;
        float scaleZ = tTrans->scale.z;
        float targetRadius = std::max(scaleX, scaleZ) * 0.5f;
        float desiredDistance = targetRadius + std::max(range - 0.2f, 0.2f);
        if (distance > desiredDistance + 0.15f) {
          desiredPos = targetPos - direction * desiredDistance;
        }
      } else {
        float desiredDistance = std::max(range - 0.2f, 0.2f);
        if (distance > desiredDistance + 0.15f) {
          desiredPos = targetPos - direction * desiredDistance;
        }
      }
    }

    CommandService::MoveOptions opts;
    opts.clearAttackIntent = false;
    opts.allowDirectFallback = true;
    std::vector<Engine::Core::EntityID> unitIds = {unitId};
    std::vector<QVector3D> moveTargets = {desiredPos};
    CommandService::moveUnits(world, unitIds, moveTargets, opts);

    auto *mv = e->getComponent<Engine::Core::MovementComponent>();
    if (!mv) {
      mv = e->addComponent<Engine::Core::MovementComponent>();
    }
    if (mv) {

      mv->targetX = desiredPos.x();
      mv->targetY = desiredPos.z();
      mv->goalX = desiredPos.x();
      mv->goalY = desiredPos.z();
      mv->hasTarget = true;
      mv->path.clear();
    }
  }
}

} // namespace Systems
} // namespace Game
