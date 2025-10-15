#include "attack_behavior.h"
#include "../../formation_system.h"
#include "../../nation_registry.h"
#include "../ai_tactical.h"
#include "../ai_utils.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Systems::AI {

void AttackBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_attackTimer += deltaTime;
  m_targetLockDuration += deltaTime;

  if (m_attackTimer < 1.5f)
    return;
  m_attackTimer = 0.0f;

  if (snapshot.visibleEnemies.empty())
    return;

  std::vector<const EntitySnapshot *> engagedUnits;
  std::vector<const EntitySnapshot *> readyUnits;
  engagedUnits.reserve(snapshot.friendlies.size());
  readyUnits.reserve(snapshot.friendlies.size());

  float groupCenterX = 0.0f;
  float groupCenterY = 0.0f;
  float groupCenterZ = 0.0f;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;

    if (isEntityEngaged(entity, snapshot.visibleEnemies)) {
      engagedUnits.push_back(&entity);
      continue;
    }

    readyUnits.push_back(&entity);
    groupCenterX += entity.posX;
    groupCenterY += entity.posY;
    groupCenterZ += entity.posZ;
  }

  if (readyUnits.empty()) {

    if (!engagedUnits.empty()) {
    }
    return;
  }

  float invCount = 1.0f / static_cast<float>(readyUnits.size());
  groupCenterX *= invCount;
  groupCenterY *= invCount;
  groupCenterZ *= invCount;

  std::vector<const ContactSnapshot *> nearbyEnemies;
  nearbyEnemies.reserve(snapshot.visibleEnemies.size());

  const float ENGAGEMENT_RANGE =
      (context.damagedUnitsCount > 0) ? 35.0f : 20.0f;
  const float engageRangeSq = ENGAGEMENT_RANGE * ENGAGEMENT_RANGE;

  for (const auto &enemy : snapshot.visibleEnemies) {
    float distSq = distanceSquared(enemy.posX, enemy.posY, enemy.posZ,
                                   groupCenterX, groupCenterY, groupCenterZ);
    if (distSq <= engageRangeSq) {
      nearbyEnemies.push_back(&enemy);
    }
  }

  if (nearbyEnemies.empty()) {

    bool shouldAdvance =
        (context.state == AIState::Attacking) ||
        (context.state == AIState::Gathering && context.totalUnits >= 3);

    if (shouldAdvance && !snapshot.visibleEnemies.empty()) {

      const ContactSnapshot *targetBarracks = nullptr;
      float closestBarracksDistSq = std::numeric_limits<float>::max();

      for (const auto &enemy : snapshot.visibleEnemies) {
        if (enemy.isBuilding) {
          float distSq =
              distanceSquared(enemy.posX, enemy.posY, enemy.posZ, groupCenterX,
                              groupCenterY, groupCenterZ);
          if (distSq < closestBarracksDistSq) {
            closestBarracksDistSq = distSq;
            targetBarracks = &enemy;
          }
        }
      }

      const ContactSnapshot *closestEnemy = nullptr;
      float closestDistSq = std::numeric_limits<float>::max();

      if (!targetBarracks) {
        for (const auto &enemy : snapshot.visibleEnemies) {
          float distSq =
              distanceSquared(enemy.posX, enemy.posY, enemy.posZ, groupCenterX,
                              groupCenterY, groupCenterZ);
          if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestEnemy = &enemy;
          }
        }
      }

      const ContactSnapshot *target =
          targetBarracks ? targetBarracks : closestEnemy;

      if (target && readyUnits.size() >= 1) {

        float attackPosX = target->posX;
        float attackPosZ = target->posZ;

        if (targetBarracks) {

          float dx = groupCenterX - target->posX;
          float dz = groupCenterZ - target->posZ;
          float dist = std::sqrt(dx * dx + dz * dz);
          if (dist > 0.1f) {
            attackPosX += (dx / dist) * 3.0f;
            attackPosZ += (dz / dist) * 3.0f;
          } else {
            attackPosX += 3.0f;
          }
        }

        bool needsNewCommand = false;
        if (m_lastTarget != target->id) {
          needsNewCommand = true;
          m_lastTarget = target->id;
          m_targetLockDuration = 0.0f;
        } else {

          for (const auto *unit : readyUnits) {
            float dx = unit->posX - attackPosX;
            float dz = unit->posZ - attackPosZ;
            float distSq = dx * dx + dz * dz;
            if (distSq > 15.0f * 15.0f) {
              needsNewCommand = true;
              break;
            }
          }
        }

        if (needsNewCommand) {
          const Nation *nation =
              NationRegistry::instance().getNationForPlayer(context.playerId);
          FormationType formationType = FormationType::Roman;
          if (nation) {
            formationType = nation->formationType;
          }

          QVector3D attackCenter(attackPosX, 0.0f, attackPosZ);
          auto formationTargets =
              FormationSystem::instance().getFormationPositions(
                  formationType, static_cast<int>(readyUnits.size()),
                  attackCenter, 1.8f);

          std::vector<Engine::Core::EntityID> unitIds;
          std::vector<float> targetX, targetY, targetZ;
          unitIds.reserve(readyUnits.size());
          targetX.reserve(readyUnits.size());
          targetY.reserve(readyUnits.size());
          targetZ.reserve(readyUnits.size());

          for (size_t i = 0; i < readyUnits.size(); ++i) {
            const auto *unit = readyUnits[i];
            const auto &target = formationTargets[i];

            unitIds.push_back(unit->id);
            targetX.push_back(target.x());
            targetY.push_back(target.y());
            targetZ.push_back(target.z());
          }

          auto claimedUnits =
              claimUnits(unitIds, BehaviorPriority::Normal, "advancing",
                         context, m_attackTimer + deltaTime, 2.0f);

          if (!claimedUnits.empty()) {
            std::vector<float> filteredX, filteredY, filteredZ;
            for (size_t i = 0; i < unitIds.size(); ++i) {
              if (std::find(claimedUnits.begin(), claimedUnits.end(),
                            unitIds[i]) != claimedUnits.end()) {
                filteredX.push_back(targetX[i]);
                filteredY.push_back(targetY[i]);
                filteredZ.push_back(targetZ[i]);
              }
            }

            AICommand cmd;
            cmd.type = AICommandType::MoveUnits;
            cmd.units = std::move(claimedUnits);
            cmd.moveTargetX = std::move(filteredX);
            cmd.moveTargetY = std::move(filteredY);
            cmd.moveTargetZ = std::move(filteredZ);
            outCommands.push_back(cmd);
          }
        }
      }
    }

    m_lastTarget = 0;
    m_targetLockDuration = 0.0f;
    return;
  }

  auto assessment = TacticalUtils::assessEngagement(
      readyUnits, nearbyEnemies,
      context.state == AIState::Attacking ? 0.7f : 0.9f);

  bool beingAttacked = context.damagedUnitsCount > 0;

  if (!assessment.shouldEngage && !context.barracksUnderThreat &&
      !beingAttacked) {

    m_lastTarget = 0;
    m_targetLockDuration = 0.0f;
    return;
  }

  bool lastTargetStillValid = false;
  if (m_lastTarget != 0) {
    for (const auto *enemy : nearbyEnemies) {
      if (enemy->id == m_lastTarget) {
        lastTargetStillValid = true;
        break;
      }
    }
  }

  if (!lastTargetStillValid || m_targetLockDuration > 8.0f) {
    m_lastTarget = 0;
    m_targetLockDuration = 0.0f;
  }

  auto targetInfo = TacticalUtils::selectFocusFireTarget(
      readyUnits, nearbyEnemies, groupCenterX, groupCenterY, groupCenterZ,
      context, m_lastTarget);

  if (targetInfo.targetId == 0)
    return;

  if (targetInfo.targetId != m_lastTarget) {
    m_lastTarget = targetInfo.targetId;
    m_targetLockDuration = 0.0f;
  }

  std::vector<Engine::Core::EntityID> unitIds;
  unitIds.reserve(readyUnits.size());
  for (const auto *unit : readyUnits) {
    unitIds.push_back(unit->id);
  }

  auto claimedUnits = claimUnits(unitIds, getPriority(), "attacking", context,
                                 m_attackTimer + deltaTime, 2.5f);

  if (claimedUnits.empty())
    return;

  AICommand command;
  command.type = AICommandType::AttackTarget;
  command.units = std::move(claimedUnits);
  command.targetId = targetInfo.targetId;

  command.shouldChase =
      (context.state == AIState::Attacking || context.barracksUnderThreat) &&
      assessment.forceRatio >= 0.8f;

  outCommands.push_back(std::move(command));
}
bool AttackBehavior::shouldExecute(const AISnapshot &snapshot,
                                   const AIContext &context) const {

  if (context.state == AIState::Retreating)
    return false;

  int readyUnits = 0;
  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding)
      continue;

    if (isEntityEngaged(entity, snapshot.visibleEnemies))
      continue;

    ++readyUnits;
  }

  if (readyUnits == 0)
    return false;

  if (snapshot.visibleEnemies.empty())
    return false;

  if (context.state == AIState::Attacking)
    return true;

  if (context.state == AIState::Defending) {
    return context.barracksUnderThreat && readyUnits >= 2;
  }

  return readyUnits >= 1;
}

} // namespace Game::Systems::AI
