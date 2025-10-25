#include "retreat_behavior.h"
#include "../../formation_planner.h"
#include "../ai_tactical.h"
#include "../ai_utils.h"

#include <QVector3D>
#include <algorithm>

namespace Game::Systems::AI {

void RetreatBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                              float deltaTime,
                              std::vector<AICommand> &outCommands) {
  m_retreatTimer += deltaTime;
  if (m_retreatTimer < 1.0f) {
    return;
  }
  m_retreatTimer = 0.0f;

  if (context.primaryBarracks == 0) {
    return;
  }

  std::vector<const EntitySnapshot *> retreatingUnits;
  retreatingUnits.reserve(snapshot.friendlies.size());

  constexpr float CRITICAL_HEALTH = 0.35f;
  constexpr float LOW_HEALTH = 0.50f;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      continue;
    }

    if (entity.maxHealth <= 0) {
      continue;
    }

    float healthRatio = static_cast<float>(entity.health) /
                        static_cast<float>(entity.maxHealth);

    if (healthRatio < CRITICAL_HEALTH) {
      retreatingUnits.push_back(&entity);
    }

    else if (healthRatio < LOW_HEALTH &&
             isEntityEngaged(entity, snapshot.visibleEnemies)) {
      retreatingUnits.push_back(&entity);
    }
  }

  if (retreatingUnits.empty()) {
    return;
  }

  QVector3D retreatPos(context.basePosX, context.basePosY, context.basePosZ);

  retreatPos.setX(retreatPos.x() - 8.0f);

  auto retreatTargets = FormationPlanner::spreadFormation(
      static_cast<int>(retreatingUnits.size()), retreatPos, 2.0f);

  std::vector<Engine::Core::EntityID> unitIds;
  std::vector<float> targetX, targetY, targetZ;
  unitIds.reserve(retreatingUnits.size());
  targetX.reserve(retreatingUnits.size());
  targetY.reserve(retreatingUnits.size());
  targetZ.reserve(retreatingUnits.size());

  for (size_t i = 0; i < retreatingUnits.size(); ++i) {
    unitIds.push_back(retreatingUnits[i]->id);
    targetX.push_back(retreatTargets[i].x());
    targetY.push_back(retreatTargets[i].y());
    targetZ.push_back(retreatTargets[i].z());
  }

  auto claimedUnits = claimUnits(unitIds, getPriority(), "retreating", context,
                                 m_retreatTimer + deltaTime, 1.0f);

  if (claimedUnits.empty()) {
    return;
  }

  std::vector<float> filteredX, filteredY, filteredZ;
  for (size_t i = 0; i < unitIds.size(); ++i) {
    if (std::find(claimedUnits.begin(), claimedUnits.end(), unitIds[i]) !=
        claimedUnits.end()) {
      filteredX.push_back(targetX[i]);
      filteredY.push_back(targetY[i]);
      filteredZ.push_back(targetZ[i]);
    }
  }

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(claimedUnits);
  command.moveTargetX = std::move(filteredX);
  command.moveTargetY = std::move(filteredY);
  command.moveTargetZ = std::move(filteredZ);
  outCommands.push_back(std::move(command));
}

bool RetreatBehavior::shouldExecute(const AISnapshot &snapshot,
                                    const AIContext &context) const {
  if (context.primaryBarracks == 0) {
    return false;
  }

  if (context.state == AIState::Retreating) {
    return true;
  }

  constexpr float CRITICAL_HEALTH = 0.35f;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      continue;
    }

    if (entity.maxHealth > 0) {
      float healthRatio = static_cast<float>(entity.health) /
                          static_cast<float>(entity.maxHealth);
      if (healthRatio < CRITICAL_HEALTH) {
        return true;
      }
    }
  }

  return false;
}

} // namespace Game::Systems::AI
