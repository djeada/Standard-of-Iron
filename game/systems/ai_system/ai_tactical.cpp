#include "ai_tactical.h"
#include "../../units/troop_type.h"
#include "../nation_registry.h"
#include "ai_utils.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems::AI {

TacticalUtils::EngagementAssessment TacticalUtils::assessEngagement(
    const std::vector<const EntitySnapshot *> &friendlies,
    const std::vector<const ContactSnapshot *> &enemies, float minForceRatio) {

  EngagementAssessment result;

  if (friendlies.empty() || enemies.empty()) {
    result.shouldEngage = false;
    return result;
  }

  result.friendlyCount = static_cast<int>(friendlies.size());
  result.enemyCount = static_cast<int>(enemies.size());

  float totalFriendlyHealth = 0.0f;
  float totalEnemyHealth = 0.0f;
  int validFriendlies = 0;
  int validEnemies = 0;

  for (const auto *unit : friendlies) {
    if (unit->maxHealth > 0) {
      totalFriendlyHealth += static_cast<float>(unit->health) /
                             static_cast<float>(unit->maxHealth);
      ++validFriendlies;
    }
  }

  for (const auto *enemy : enemies) {
    if (enemy->maxHealth > 0) {
      totalEnemyHealth += static_cast<float>(enemy->health) /
                          static_cast<float>(enemy->maxHealth);
      ++validEnemies;
    }
  }

  result.avgFriendlyHealth =
      validFriendlies > 0 ? (totalFriendlyHealth / validFriendlies) : 1.0f;
  result.avgEnemyHealth =
      validEnemies > 0 ? (totalEnemyHealth / validEnemies) : 1.0f;

  float friendlyStrength =
      static_cast<float>(result.friendlyCount) * result.avgFriendlyHealth;
  float enemyStrength =
      static_cast<float>(result.enemyCount) * result.avgEnemyHealth;

  if (enemyStrength < 0.01f) {
    result.forceRatio = 10.0f;
  } else {
    result.forceRatio = friendlyStrength / enemyStrength;
  }

  result.confidenceLevel =
      std::clamp((result.forceRatio - 0.5f) / 1.5f, 0.0f, 1.0f);

  result.shouldEngage = (result.forceRatio >= minForceRatio);

  return result;
}

TacticalUtils::TargetScore TacticalUtils::selectFocusFireTarget(
    const std::vector<const EntitySnapshot *> &attackers,
    const std::vector<const ContactSnapshot *> &enemies, float groupCenterX,
    float groupCenterY, float groupCenterZ, const AIContext &context,
    Engine::Core::EntityID currentTarget) {

  TargetScore bestTarget;
  bestTarget.score = -std::numeric_limits<float>::infinity();

  if (enemies.empty()) {
    return bestTarget;
  }

  for (const auto *enemy : enemies) {
    float score = 0.0f;

    float dist = distance(enemy->posX, enemy->posY, enemy->posZ, groupCenterX,
                          groupCenterY, groupCenterZ);
    score -= dist * 0.5f;

    if (enemy->maxHealth > 0) {
      float healthRatio = static_cast<float>(enemy->health) /
                          static_cast<float>(enemy->maxHealth);

      if (healthRatio < 0.5f) {
        score += 8.0f * (1.0f - healthRatio);
      }

      if (healthRatio < 0.25f) {
        score += 12.0f;
      }
    }

    float typePriority = getUnitTypePriority(
        Game::Units::spawnTypeToString(enemy->spawnType), context.nation);
    score += typePriority * 3.0f;

    if (!enemy->isBuilding) {
      score += 5.0f;
    }

    if (currentTarget != 0 && enemy->id == currentTarget) {
      score += 10.0f;
    }

    bool isolated = isTargetIsolated(*enemy, enemies, 8.0f);
    if (isolated) {
      score += 6.0f;
    }

    if (context.primaryBarracks != 0) {
      float distToBase =
          distance(enemy->posX, enemy->posY, enemy->posZ, context.basePosX,
                   context.basePosY, context.basePosZ);

      if (distToBase < 16.0f) {
        score += (16.0f - distToBase) * 0.8f;
      }
    }

    if (context.state == AIState::Attacking && !enemy->isBuilding) {
      score += 3.0f;
    }

    if (score > bestTarget.score) {
      bestTarget.targetId = enemy->id;
      bestTarget.score = score;
      bestTarget.distanceToGroup = dist;
      bestTarget.isLowHealth =
          (enemy->maxHealth > 0 && enemy->health < enemy->maxHealth / 2);
      bestTarget.isIsolated = isolated;
    }
  }

  return bestTarget;
}

float TacticalUtils::calculateForceStrength(
    const std::vector<const EntitySnapshot *> &units) {

  float strength = 0.0f;
  for (const auto *unit : units) {
    if (unit->maxHealth > 0) {
      float healthRatio = static_cast<float>(unit->health) /
                          static_cast<float>(unit->maxHealth);
      strength += healthRatio;
    } else {
      strength += 1.0f;
    }
  }
  return strength;
}

float TacticalUtils::calculateForceStrength(
    const std::vector<const ContactSnapshot *> &units) {

  float strength = 0.0f;
  for (const auto *unit : units) {
    if (unit->maxHealth > 0) {
      float healthRatio = static_cast<float>(unit->health) /
                          static_cast<float>(unit->maxHealth);
      strength += healthRatio;
    } else {
      strength += 1.0f;
    }
  }
  return strength;
}

bool TacticalUtils::isTargetIsolated(
    const ContactSnapshot &target,
    const std::vector<const ContactSnapshot *> &allEnemies,
    float isolationRadius) {

  const float isolationRadiusSq = isolationRadius * isolationRadius;
  int nearbyAllies = 0;

  for (const auto *enemy : allEnemies) {

    if (enemy->id == target.id) {
      continue;
    }

    float distSq = distanceSquared(target.posX, target.posY, target.posZ,
                                   enemy->posX, enemy->posY, enemy->posZ);

    if (distSq <= isolationRadiusSq) {
      ++nearbyAllies;
    }
  }

  return (nearbyAllies <= 1);
}

float TacticalUtils::getUnitTypePriority(const std::string &unitType,
                                         const Game::Systems::Nation *nation) {

  if (nation) {
    auto troopType = Game::Units::troopTypeFromString(unitType);
    if (nation->isRangedUnit(troopType)) {
      return 3.0f;
    }
    if (nation->isMeleeUnit(troopType)) {
      return 2.0f;
    }
  }

  if (unitType == "worker" || unitType == "villager") {
    return 1.0f;
  }

  if (unitType == "barracks" || unitType == "base") {
    return 0.5f;
  }

  return 1.5f;
}

} // namespace Game::Systems::AI
