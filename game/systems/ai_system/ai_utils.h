#pragma once

#include "ai_types.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace Game::Systems::AI {

inline void replicateLastTargetIfNeeded(const std::vector<float> &fromX,
                                        const std::vector<float> &fromY,
                                        const std::vector<float> &fromZ,
                                        size_t wanted, std::vector<float> &outX,
                                        std::vector<float> &outY,
                                        std::vector<float> &outZ) {

  outX.clear();
  outY.clear();
  outZ.clear();

  if (fromX.empty() || fromY.empty() || fromZ.empty())
    return;

  size_t srcSize = std::min({fromX.size(), fromY.size(), fromZ.size()});

  outX.reserve(wanted);
  outY.reserve(wanted);
  outZ.reserve(wanted);

  for (size_t i = 0; i < wanted; ++i) {
    size_t idx = std::min(i, srcSize - 1);
    outX.push_back(fromX[idx]);
    outY.push_back(fromY[idx]);
    outZ.push_back(fromZ[idx]);
  }
}

inline bool isEntityEngaged(const EntitySnapshot &entity,
                            const std::vector<ContactSnapshot> &enemies) {

  if (entity.maxHealth > 0 && entity.health < entity.maxHealth)
    return true;

  constexpr float ENGAGED_RADIUS = 7.5f;
  const float engagedSq = ENGAGED_RADIUS * ENGAGED_RADIUS;

  for (const auto &enemy : enemies) {
    float dx = enemy.posX - entity.posX;
    float dy = enemy.posY - entity.posY;
    float dz = enemy.posZ - entity.posZ;
    float distSq = dx * dx + dy * dy + dz * dz;

    if (distSq <= engagedSq)
      return true;
  }

  return false;
}

inline float distanceSquared(float x1, float y1, float z1, float x2, float y2,
                             float z2) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float dz = z2 - z1;
  return dx * dx + dy * dy + dz * dz;
}

inline float distance(float x1, float y1, float z1, float x2, float y2,
                      float z2) {
  return std::sqrt(distanceSquared(x1, y1, z1, x2, y2, z2));
}

inline std::vector<Engine::Core::EntityID>
claimUnits(const std::vector<Engine::Core::EntityID> &requestedUnits,
           BehaviorPriority priority, const std::string &taskName,
           AIContext &context, float currentTime,
           float minLockDuration = 2.0f) {

  std::vector<Engine::Core::EntityID> claimed;
  claimed.reserve(requestedUnits.size());

  for (Engine::Core::EntityID unitId : requestedUnits) {
    auto it = context.assignedUnits.find(unitId);

    if (it == context.assignedUnits.end()) {

      AIContext::UnitAssignment assignment;
      assignment.ownerPriority = priority;
      assignment.assignmentTime = currentTime;
      assignment.assignedTask = taskName;
      context.assignedUnits[unitId] = assignment;
      claimed.push_back(unitId);

    } else {

      const auto &existing = it->second;
      float assignmentAge = currentTime - existing.assignmentTime;

      bool canSteal = (priority > existing.ownerPriority) &&
                      (assignmentAge > minLockDuration);

      if (canSteal) {

        AIContext::UnitAssignment assignment;
        assignment.ownerPriority = priority;
        assignment.assignmentTime = currentTime;
        assignment.assignedTask = taskName;
        context.assignedUnits[unitId] = assignment;
        claimed.push_back(unitId);
      }
    }
  }

  return claimed;
}

inline void releaseUnits(const std::vector<Engine::Core::EntityID> &units,
                         AIContext &context) {
  for (Engine::Core::EntityID unitId : units) {
    context.assignedUnits.erase(unitId);
  }
}

inline void cleanupDeadUnits(const AISnapshot &snapshot, AIContext &context) {

  std::unordered_set<Engine::Core::EntityID> aliveUnits;
  for (const auto &entity : snapshot.friendlies) {
    if (!entity.isBuilding) {
      aliveUnits.insert(entity.id);
    }
  }

  for (auto it = context.assignedUnits.begin();
       it != context.assignedUnits.end();) {
    if (aliveUnits.find(it->first) == aliveUnits.end()) {
      it = context.assignedUnits.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace Game::Systems::AI
