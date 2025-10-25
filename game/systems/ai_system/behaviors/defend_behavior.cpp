#include "defend_behavior.h"
#include "../../formation_planner.h"
#include "../../formation_system.h"
#include "../../nation_registry.h"
#include "../ai_tactical.h"
#include "../ai_utils.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Systems::AI {

void DefendBehavior::execute(const AISnapshot &snapshot, AIContext &context,
                             float deltaTime,
                             std::vector<AICommand> &outCommands) {
  m_defendTimer += deltaTime;

  float updateInterval = context.barracksUnderThreat ? 0.5f : 1.5f;

  if (m_defendTimer < updateInterval) {
    return;
  }
  m_defendTimer = 0.0f;

  if (context.primaryBarracks == 0) {
    return;
  }

  float defendPosX = 0.0f;
  float defendPosY = 0.0f;
  float defendPosZ = 0.0f;
  bool foundBarracks = false;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.id == context.primaryBarracks) {
      defendPosX = entity.posX;
      defendPosY = entity.posY;
      defendPosZ = entity.posZ;
      foundBarracks = true;
      break;
    }
  }

  if (!foundBarracks) {
    return;
  }

  std::vector<const EntitySnapshot *> readyDefenders;
  std::vector<const EntitySnapshot *> engagedDefenders;
  readyDefenders.reserve(snapshot.friendlies.size());
  engagedDefenders.reserve(snapshot.friendlies.size());

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      continue;
    }

    if (isEntityEngaged(entity, snapshot.visibleEnemies)) {
      engagedDefenders.push_back(&entity);
    } else {
      readyDefenders.push_back(&entity);
    }
  }

  if (readyDefenders.empty() && engagedDefenders.empty()) {
    return;
  }

  auto sortByDistance = [&](std::vector<const EntitySnapshot *> &list) {
    std::sort(list.begin(), list.end(),
              [&](const EntitySnapshot *a, const EntitySnapshot *b) {
                float da = distanceSquared(a->posX, a->posY, a->posZ,
                                           defendPosX, defendPosY, defendPosZ);
                float db = distanceSquared(b->posX, b->posY, b->posZ,
                                           defendPosX, defendPosY, defendPosZ);
                return da < db;
              });
  };

  sortByDistance(readyDefenders);
  sortByDistance(engagedDefenders);

  const std::size_t totalAvailable =
      readyDefenders.size() + engagedDefenders.size();
  std::size_t desiredCount = totalAvailable;

  if (context.barracksUnderThreat || !context.buildingsUnderAttack.empty()) {

    desiredCount = totalAvailable;
  } else {

    desiredCount =
        std::min<std::size_t>(desiredCount, static_cast<std::size_t>(6));
  }

  std::size_t readyCount = std::min(desiredCount, readyDefenders.size());
  readyDefenders.resize(readyCount);

  if (readyDefenders.empty()) {
    return;
  }

  if (context.barracksUnderThreat || !context.buildingsUnderAttack.empty()) {

    std::vector<const ContactSnapshot *> nearbyThreats;
    nearbyThreats.reserve(snapshot.visibleEnemies.size());

    constexpr float DEFEND_RADIUS = 40.0f;
    const float defendRadiusSq = DEFEND_RADIUS * DEFEND_RADIUS;

    for (const auto &enemy : snapshot.visibleEnemies) {
      float distSq = distanceSquared(enemy.posX, enemy.posY, enemy.posZ,
                                     defendPosX, defendPosY, defendPosZ);
      if (distSq <= defendRadiusSq) {
        nearbyThreats.push_back(&enemy);
      }
    }

    if (!nearbyThreats.empty()) {

      auto targetInfo = TacticalUtils::selectFocusFireTarget(
          readyDefenders, nearbyThreats, defendPosX, defendPosY, defendPosZ,
          context, 0);

      if (targetInfo.targetId != 0) {

        std::vector<Engine::Core::EntityID> defenderIds;
        defenderIds.reserve(readyDefenders.size());
        for (const auto *unit : readyDefenders) {
          defenderIds.push_back(unit->id);
        }

        auto claimedUnits =
            claimUnits(defenderIds, getPriority(), "defending", context,
                       m_defendTimer + deltaTime, 3.0f);

        if (!claimedUnits.empty()) {
          AICommand attack;
          attack.type = AICommandType::AttackTarget;
          attack.units = std::move(claimedUnits);
          attack.targetId = targetInfo.targetId;
          attack.shouldChase = true;
          outCommands.push_back(std::move(attack));
          return;
        }
      }
    } else if (context.barracksUnderThreat ||
               !context.buildingsUnderAttack.empty()) {

      const ContactSnapshot *closestThreat = nullptr;
      float closestDistSq = std::numeric_limits<float>::max();

      for (const auto &enemy : snapshot.visibleEnemies) {
        float distSq = distanceSquared(enemy.posX, enemy.posY, enemy.posZ,
                                       defendPosX, defendPosY, defendPosZ);
        if (distSq < closestDistSq) {
          closestDistSq = distSq;
          closestThreat = &enemy;
        }
      }

      if (closestThreat && !readyDefenders.empty()) {

        std::vector<Engine::Core::EntityID> defenderIds;
        std::vector<float> targetX, targetY, targetZ;

        for (const auto *unit : readyDefenders) {
          defenderIds.push_back(unit->id);
          targetX.push_back(closestThreat->posX);
          targetY.push_back(closestThreat->posY);
          targetZ.push_back(closestThreat->posZ);
        }

        auto claimedUnits =
            claimUnits(defenderIds, getPriority(), "intercepting", context,
                       m_defendTimer + deltaTime, 2.0f);

        if (!claimedUnits.empty()) {

          std::vector<float> filteredX, filteredY, filteredZ;
          for (size_t i = 0; i < defenderIds.size(); ++i) {
            if (std::find(claimedUnits.begin(), claimedUnits.end(),
                          defenderIds[i]) != claimedUnits.end()) {
              filteredX.push_back(targetX[i]);
              filteredY.push_back(targetY[i]);
              filteredZ.push_back(targetZ[i]);
            }
          }

          AICommand move;
          move.type = AICommandType::MoveUnits;
          move.units = std::move(claimedUnits);
          move.moveTargetX = std::move(filteredX);
          move.moveTargetY = std::move(filteredY);
          move.moveTargetZ = std::move(filteredZ);
          outCommands.push_back(std::move(move));
          return;
        }
      }
    }
  }

  std::vector<const EntitySnapshot *> unclaimedDefenders;
  for (const auto *unit : readyDefenders) {
    auto it = context.assignedUnits.find(unit->id);
    if (it == context.assignedUnits.end()) {
      unclaimedDefenders.push_back(unit);
    }
  }

  if (unclaimedDefenders.empty()) {
    return;
  }

  const Nation *nation =
      NationRegistry::instance().getNationForPlayer(context.playerId);
  FormationType formationType = FormationType::Roman;
  if (nation) {
    formationType = nation->formationType;
  }

  QVector3D defendPos(defendPosX, defendPosY, defendPosZ);
  auto targets = FormationSystem::instance().getFormationPositions(
      formationType, static_cast<int>(unclaimedDefenders.size()), defendPos,
      3.0f);

  std::vector<Engine::Core::EntityID> unitsToMove;
  std::vector<float> targetX, targetY, targetZ;
  unitsToMove.reserve(unclaimedDefenders.size());
  targetX.reserve(unclaimedDefenders.size());
  targetY.reserve(unclaimedDefenders.size());
  targetZ.reserve(unclaimedDefenders.size());

  for (size_t i = 0; i < unclaimedDefenders.size(); ++i) {
    const auto *entity = unclaimedDefenders[i];
    const auto &target = targets[i];

    float dx = entity->posX - target.x();
    float dz = entity->posZ - target.z();
    float distanceSq = dx * dx + dz * dz;

    if (distanceSq < 1.0f * 1.0f) {
      continue;
    }

    unitsToMove.push_back(entity->id);
    targetX.push_back(target.x());
    targetY.push_back(target.y());
    targetZ.push_back(target.z());
  }

  if (unitsToMove.empty()) {
    return;
  }

  auto claimedForMove =
      claimUnits(unitsToMove, BehaviorPriority::Low, "positioning", context,
                 m_defendTimer + deltaTime, 1.5f);

  if (claimedForMove.empty()) {
    return;
  }

  std::vector<float> filteredX, filteredY, filteredZ;
  for (size_t i = 0; i < unitsToMove.size(); ++i) {
    if (std::find(claimedForMove.begin(), claimedForMove.end(),
                  unitsToMove[i]) != claimedForMove.end()) {
      filteredX.push_back(targetX[i]);
      filteredY.push_back(targetY[i]);
      filteredZ.push_back(targetZ[i]);
    }
  }

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.units = std::move(claimedForMove);
  command.moveTargetX = std::move(filteredX);
  command.moveTargetY = std::move(filteredY);
  command.moveTargetZ = std::move(filteredZ);
  outCommands.push_back(std::move(command));
}

bool DefendBehavior::shouldExecute(const AISnapshot &snapshot,
                                   const AIContext &context) const {
  (void)snapshot;

  if (context.primaryBarracks == 0) {
    return false;
  }

  if (context.barracksUnderThreat || !context.buildingsUnderAttack.empty()) {
    return true;
  }

  if (context.state == AIState::Defending && context.idleUnits > 0) {
    return true;
  }

  if (context.averageHealth < 0.6f && context.totalUnits > 0) {
    return true;
  }

  return false;
}

} // namespace Game::Systems::AI
