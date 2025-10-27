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

  if (fromX.empty() || fromY.empty() || fromZ.empty()) {
    return;
  }

  size_t const srcSize = std::min({fromX.size(), fromY.size(), fromZ.size()});

  outX.reserve(wanted);
  outY.reserve(wanted);
  outZ.reserve(wanted);

  for (size_t i = 0; i < wanted; ++i) {
    size_t const idx = std::min(i, srcSize - 1);
    outX.push_back(fromX[idx]);
    outY.push_back(fromY[idx]);
    outZ.push_back(fromZ[idx]);
  }
}

inline auto
isEntityEngaged(const EntitySnapshot &entity,
                const std::vector<ContactSnapshot> &enemies) -> bool {

  if (entity.max_health > 0 && entity.health < entity.max_health) {
    return true;
  }

  constexpr float ENGAGED_RADIUS = 7.5F;
  const float engagedSq = ENGAGED_RADIUS * ENGAGED_RADIUS;

  for (const auto &enemy : enemies) {
    float const dx = enemy.posX - entity.posX;
    float const dy = enemy.posY - entity.posY;
    float const dz = enemy.posZ - entity.posZ;
    float const dist_sq = dx * dx + dy * dy + dz * dz;

    if (dist_sq <= engagedSq) {
      return true;
    }
  }

  return false;
}

inline auto distance_squared(float x1, float y1, float z1, float x2, float y2,
                             float z2) -> float {
  float const dx = x2 - x1;
  float const dy = y2 - y1;
  float const dz = z2 - z1;
  return dx * dx + dy * dy + dz * dz;
}

inline auto distance(float x1, float y1, float z1, float x2, float y2,
                     float z2) -> float {
  return std::sqrt(distance_squared(x1, y1, z1, x2, y2, z2));
}

inline auto claimUnits(
    const std::vector<Engine::Core::EntityID> &requestedUnits,
    BehaviorPriority priority, const std::string &taskName, AIContext &context,
    float currentTime,
    float minLockDuration = 2.0F) -> std::vector<Engine::Core::EntityID> {

  std::vector<Engine::Core::EntityID> claimed;
  claimed.reserve(requestedUnits.size());

  for (Engine::Core::EntityID const unit_id : requestedUnits) {
    auto it = context.assignedUnits.find(unit_id);

    if (it == context.assignedUnits.end()) {

      AIContext::UnitAssignment assignment;
      assignment.ownerPriority = priority;
      assignment.assignmentTime = currentTime;
      assignment.assignedTask = taskName;
      context.assignedUnits[unit_id] = assignment;
      claimed.push_back(unit_id);

    } else {

      const auto &existing = it->second;
      float const assignmentAge = currentTime - existing.assignmentTime;

      bool const canSteal = (priority > existing.ownerPriority) &&
                            (assignmentAge > minLockDuration);

      if (canSteal) {

        AIContext::UnitAssignment assignment;
        assignment.ownerPriority = priority;
        assignment.assignmentTime = currentTime;
        assignment.assignedTask = taskName;
        context.assignedUnits[unit_id] = assignment;
        claimed.push_back(unit_id);
      }
    }
  }

  return claimed;
}

inline void releaseUnits(const std::vector<Engine::Core::EntityID> &units,
                         AIContext &context) {
  for (Engine::Core::EntityID const unit_id : units) {
    context.assignedUnits.erase(unit_id);
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
