#include "ai_reasoner.h"
#include "../../game_config.h"
#include "../../units/troop_type.h"
#include "../nation_registry.h"
#include "ai_utils.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Systems::AI {

void AIReasoner::updateContext(const AISnapshot &snapshot, AIContext &ctx) {

  if (!ctx.nation) {
    ctx.nation = Game::Systems::NationRegistry::instance().getNationForPlayer(
        ctx.playerId);
  }

  cleanupDeadUnits(snapshot, ctx);

  ctx.militaryUnits.clear();
  ctx.buildings.clear();
  ctx.primaryBarracks = 0;
  ctx.totalUnits = 0;
  ctx.idleUnits = 0;
  ctx.combatUnits = 0;
  ctx.meleeCount = 0;
  ctx.rangedCount = 0;
  ctx.damagedUnitsCount = 0;
  ctx.averageHealth = 1.0f;
  ctx.rallyX = 0.0f;
  ctx.rallyZ = 0.0f;
  ctx.barracksUnderThreat = false;
  ctx.nearbyThreatCount = 0;
  ctx.closestThreatDistance = std::numeric_limits<float>::infinity();
  ctx.basePosX = 0.0f;
  ctx.basePosY = 0.0f;
  ctx.basePosZ = 0.0f;
  ctx.visibleEnemyCount = 0;
  ctx.enemyBuildingsCount = 0;
  ctx.averageEnemyDistance = 0.0f;
  ctx.maxTroopsPerPlayer = Game::GameConfig::instance().getMaxTroopsPerPlayer();

  constexpr float ATTACK_RECORD_TIMEOUT = 10.0f;
  auto it = ctx.buildingsUnderAttack.begin();
  while (it != ctx.buildingsUnderAttack.end()) {
    bool stillExists = false;
    for (const auto &entity : snapshot.friendlies) {
      if (entity.id == it->first && entity.isBuilding) {
        stillExists = true;
        break;
      }
    }

    if (!stillExists ||
        (snapshot.gameTime - it->second) > ATTACK_RECORD_TIMEOUT) {
      it = ctx.buildingsUnderAttack.erase(it);
    } else {
      ++it;
    }
  }

  float totalHealthRatio = 0.0f;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      ctx.buildings.push_back(entity.id);

      if (entity.spawnType == Game::Units::SpawnType::Barracks && 
          ctx.primaryBarracks == 0) {
        ctx.primaryBarracks = entity.id;
        ctx.rallyX = entity.posX - 5.0f;
        ctx.rallyZ = entity.posZ;
        ctx.basePosX = entity.posX;
        ctx.basePosY = entity.posY;
        ctx.basePosZ = entity.posZ;
      }
      continue;
    }

    ctx.militaryUnits.push_back(entity.id);
    ctx.totalUnits++;

    if (ctx.nation) {
      auto troopTypeOpt = Game::Units::spawnTypeToTroopType(entity.spawnType);
      if (troopTypeOpt) {
        auto troopType = *troopTypeOpt;
        if (ctx.nation->isRangedUnit(troopType)) {
          ctx.rangedCount++;
        } else if (ctx.nation->isMeleeUnit(troopType)) {
          ctx.meleeCount++;
        }
      }
    }

    if (!entity.movement.hasComponent || !entity.movement.hasTarget) {
      ctx.idleUnits++;
    } else {
      ctx.combatUnits++;
    }

    if (entity.maxHealth > 0) {
      float healthRatio = static_cast<float>(entity.health) /
                          static_cast<float>(entity.maxHealth);
      totalHealthRatio += healthRatio;

      if (healthRatio < 0.5f) {
        ctx.damagedUnitsCount++;
      }
    }
  }

  ctx.averageHealth =
      (ctx.totalUnits > 0)
          ? (totalHealthRatio / static_cast<float>(ctx.totalUnits))
          : 1.0f;

  ctx.visibleEnemyCount = static_cast<int>(snapshot.visibleEnemies.size());
  float totalEnemyDist = 0.0f;

  for (const auto &enemy : snapshot.visibleEnemies) {
    if (enemy.isBuilding) {
      ctx.enemyBuildingsCount++;
    }

    if (ctx.primaryBarracks != 0) {
      float dist = distance(enemy.posX, enemy.posY, enemy.posZ, ctx.basePosX,
                            ctx.basePosY, ctx.basePosZ);
      totalEnemyDist += dist;
    }
  }

  ctx.averageEnemyDistance =
      (ctx.visibleEnemyCount > 0)
          ? (totalEnemyDist / static_cast<float>(ctx.visibleEnemyCount))
          : 1000.0f;

  if (ctx.primaryBarracks != 0) {

    constexpr float DEFEND_RADIUS = 40.0f;
    constexpr float CRITICAL_RADIUS = 20.0f;
    const float defendRadiusSq = DEFEND_RADIUS * DEFEND_RADIUS;
    const float criticalRadiusSq = CRITICAL_RADIUS * CRITICAL_RADIUS;

    for (const auto &enemy : snapshot.visibleEnemies) {
      float distSq = distanceSquared(enemy.posX, enemy.posY, enemy.posZ,
                                     ctx.basePosX, ctx.basePosY, ctx.basePosZ);

      if (distSq <= defendRadiusSq) {
        ctx.barracksUnderThreat = true;
        ctx.nearbyThreatCount++;

        float dist = std::sqrt(std::max(distSq, 0.0f));
        ctx.closestThreatDistance = std::min(ctx.closestThreatDistance, dist);
      }
    }

    if (!ctx.barracksUnderThreat) {
      ctx.closestThreatDistance = std::numeric_limits<float>::infinity();
    }
  }
}

void AIReasoner::updateStateMachine(AIContext &ctx, float deltaTime) {
  ctx.stateTimer += deltaTime;
  ctx.decisionTimer += deltaTime;

  constexpr float MIN_STATE_DURATION = 3.0f;

  AIState previousState = ctx.state;
  if ((ctx.barracksUnderThreat || !ctx.buildingsUnderAttack.empty()) &&
      ctx.state != AIState::Defending) {

    ctx.state = AIState::Defending;
  }

  else if (ctx.visibleEnemyCount > 0 && ctx.averageEnemyDistance < 50.0f &&
           (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
    ctx.state = AIState::Defending;
  }

  if (ctx.decisionTimer < 2.0f) {
    if (ctx.state != previousState)
      ctx.stateTimer = 0.0f;
    return;
  }

  ctx.decisionTimer = 0.0f;
  previousState = ctx.state;

  if (ctx.stateTimer < MIN_STATE_DURATION &&
      !((ctx.barracksUnderThreat || !ctx.buildingsUnderAttack.empty()) &&
        ctx.state != AIState::Defending)) {
    return;
  }

  switch (ctx.state) {
  case AIState::Idle:
    if (ctx.idleUnits >= 2) {
      ctx.state = AIState::Gathering;
    } else if (ctx.averageHealth < 0.40f && ctx.totalUnits > 0) {

      ctx.state = AIState::Defending;
    } else if (ctx.totalUnits >= 1 && ctx.visibleEnemyCount > 0) {

      ctx.state = AIState::Attacking;
    }
    break;

  case AIState::Gathering:
    if (ctx.totalUnits >= 3) {

      ctx.state = AIState::Attacking;
    } else if (ctx.totalUnits < 2) {
      ctx.state = AIState::Idle;
    } else if (ctx.averageHealth < 0.40f) {

      ctx.state = AIState::Defending;
    } else if (ctx.visibleEnemyCount > 0 && ctx.totalUnits >= 2) {

      ctx.state = AIState::Attacking;
    }
    break;

  case AIState::Attacking:
    if (ctx.averageHealth < 0.25f) {

      ctx.state = AIState::Retreating;
    } else if (ctx.totalUnits == 0) {

      ctx.state = AIState::Idle;
    }

    break;

  case AIState::Defending:

    if (ctx.barracksUnderThreat || !ctx.buildingsUnderAttack.empty()) {

    } else if (ctx.totalUnits >= 4 && ctx.averageHealth > 0.65f) {

      ctx.state = AIState::Attacking;
    } else if (ctx.averageHealth > 0.80f) {

      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Retreating:

    if (ctx.stateTimer > 6.0f && ctx.averageHealth > 0.55f) {

      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Expanding:

    ctx.state = AIState::Idle;
    break;
  }

  if (ctx.state != previousState) {
    ctx.stateTimer = 0.0f;
  }
}

} // namespace Game::Systems::AI
