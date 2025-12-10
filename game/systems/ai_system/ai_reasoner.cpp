#include "ai_reasoner.h"
#include "../../game_config.h"
#include "../nation_registry.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Systems::AI {

void AIReasoner::updateContext(const AISnapshot &snapshot, AIContext &ctx) {

  if (ctx.nation == nullptr) {
    ctx.nation = Game::Systems::NationRegistry::instance().getNationForPlayer(
        ctx.player_id);
  }

  cleanupDeadUnits(snapshot, ctx);

  ctx.militaryUnits.clear();
  ctx.buildings.clear();
  ctx.primaryBarracks = 0;
  ctx.total_units = 0;
  ctx.idleUnits = 0;
  ctx.combatUnits = 0;
  ctx.meleeCount = 0;
  ctx.rangedCount = 0;
  ctx.damagedUnitsCount = 0;
  ctx.averageHealth = 1.0F;
  ctx.rally_x = 0.0F;
  ctx.rally_z = 0.0F;
  ctx.barracksUnderThreat = false;
  ctx.nearbyThreatCount = 0;
  ctx.closest_threatDistance = std::numeric_limits<float>::infinity();
  ctx.basePosX = 0.0F;
  ctx.basePosY = 0.0F;
  ctx.basePosZ = 0.0F;
  ctx.visibleEnemyCount = 0;
  ctx.enemyBuildingsCount = 0;
  ctx.averageEnemyDistance = 0.0F;
  ctx.max_troops_per_player =
      Game::GameConfig::instance().getMaxTroopsPerPlayer();

  constexpr float attack_record_timeout = 10.0F;
  auto it = ctx.buildingsUnderAttack.begin();
  while (it != ctx.buildingsUnderAttack.end()) {
    bool still_exists = false;
    for (const auto &entity : snapshot.friendlies) {
      if (entity.id == it->first && entity.isBuilding) {
        still_exists = true;
        break;
      }
    }

    if (!still_exists ||
        (snapshot.gameTime - it->second) > attack_record_timeout) {
      it = ctx.buildingsUnderAttack.erase(it);
    } else {
      ++it;
    }
  }

  float total_health_ratio = 0.0F;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.isBuilding) {
      ctx.buildings.push_back(entity.id);

      if (entity.spawn_type == Game::Units::SpawnType::Barracks &&
          ctx.primaryBarracks == 0) {
        ctx.primaryBarracks = entity.id;
        ctx.rally_x = entity.posX - 5.0F;
        ctx.rally_z = entity.posZ;
        ctx.basePosX = entity.posX;
        ctx.basePosY = entity.posY;
        ctx.basePosZ = entity.posZ;
      }
      continue;
    }

    ctx.militaryUnits.push_back(entity.id);
    ctx.total_units++;

    if (ctx.nation != nullptr) {
      auto troop_type_opt =
          Game::Units::spawn_typeToTroopType(entity.spawn_type);
      if (troop_type_opt) {
        auto troop_type = *troop_type_opt;
        if (ctx.nation->is_ranged_unit(troop_type)) {
          ctx.rangedCount++;
        } else if (ctx.nation->isMeleeUnit(troop_type)) {
          ctx.meleeCount++;
        }
      }
    }

    if (!entity.movement.has_component || !entity.movement.has_target) {
      ctx.idleUnits++;
    } else {
      ctx.combatUnits++;
    }

    if (entity.max_health > 0) {
      float const health_ratio = static_cast<float>(entity.health) /
                                 static_cast<float>(entity.max_health);
      total_health_ratio += health_ratio;

      if (health_ratio < 0.5F) {
        ctx.damagedUnitsCount++;
      }
    }
  }

  ctx.averageHealth =
      (ctx.total_units > 0)
          ? (total_health_ratio / static_cast<float>(ctx.total_units))
          : 1.0F;

  ctx.visibleEnemyCount = static_cast<int>(snapshot.visibleEnemies.size());
  float total_enemy_dist = 0.0F;

  for (const auto &enemy : snapshot.visibleEnemies) {
    if (enemy.isBuilding) {
      ctx.enemyBuildingsCount++;
    }

    if (ctx.primaryBarracks != 0) {
      float const dist = distance(enemy.posX, enemy.posY, enemy.posZ,
                                  ctx.basePosX, ctx.basePosY, ctx.basePosZ);
      total_enemy_dist += dist;
    }
  }

  ctx.averageEnemyDistance =
      (ctx.visibleEnemyCount > 0)
          ? (total_enemy_dist / static_cast<float>(ctx.visibleEnemyCount))
          : 1000.0F;

  if (ctx.primaryBarracks != 0) {

    constexpr float defend_radius = 40.0F;
    constexpr float critical_radius = 20.0F;
    const float defend_radius_sq = defend_radius * defend_radius;
    const float critical_radius_sq = critical_radius * critical_radius;

    for (const auto &enemy : snapshot.visibleEnemies) {
      float const dist_sq =
          distance_squared(enemy.posX, enemy.posY, enemy.posZ, ctx.basePosX,
                           ctx.basePosY, ctx.basePosZ);

      if (dist_sq <= defend_radius_sq) {
        ctx.barracksUnderThreat = true;
        ctx.nearbyThreatCount++;

        float const dist = std::sqrt(std::max(dist_sq, 0.0F));
        ctx.closest_threatDistance = std::min(ctx.closest_threatDistance, dist);
      }
    }

    if (!ctx.barracksUnderThreat) {
      ctx.closest_threatDistance = std::numeric_limits<float>::infinity();
    }
  }
}

void AIReasoner::updateStateMachine(AIContext &ctx, float delta_time) {
  ctx.stateTimer += delta_time;
  ctx.decisionTimer += delta_time;

  constexpr float min_state_duration = 3.0F;

  AIState previous_state = ctx.state;
  if ((ctx.barracksUnderThreat || !ctx.buildingsUnderAttack.empty()) &&
      ctx.state != AIState::Defending) {

    ctx.state = AIState::Defending;
  }

  else if (ctx.visibleEnemyCount > 0 && ctx.averageEnemyDistance < 50.0F &&
           (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
    ctx.state = AIState::Defending;
  }

  if (ctx.decisionTimer < 2.0F) {
    if (ctx.state != previous_state) {
      ctx.stateTimer = 0.0F;
    }
    return;
  }

  ctx.decisionTimer = 0.0F;
  previous_state = ctx.state;

  if (ctx.stateTimer < min_state_duration &&
      ((!ctx.barracksUnderThreat && ctx.buildingsUnderAttack.empty()) ||
       ctx.state == AIState::Defending)) {
    return;
  }

  switch (ctx.state) {
  case AIState::Idle:
    if (ctx.idleUnits >= 2) {
      ctx.state = AIState::Gathering;
    } else if (ctx.averageHealth < 0.40F && ctx.total_units > 0) {

      ctx.state = AIState::Defending;
    } else if (ctx.total_units >= 1 && ctx.visibleEnemyCount > 0) {

      ctx.state = AIState::Attacking;
    }
    break;

  case AIState::Gathering:
    if (ctx.total_units >= 3) {

      ctx.state = AIState::Attacking;
    } else if (ctx.total_units < 2) {
      ctx.state = AIState::Idle;
    } else if (ctx.averageHealth < 0.40F) {

      ctx.state = AIState::Defending;
    } else if (ctx.visibleEnemyCount > 0 && ctx.total_units >= 2) {

      ctx.state = AIState::Attacking;
    }
    break;

  case AIState::Attacking:
    if (ctx.averageHealth < 0.25F) {

      ctx.state = AIState::Retreating;
    } else if (ctx.total_units == 0) {

      ctx.state = AIState::Idle;
    }

    break;

  case AIState::Defending:

    if (ctx.barracksUnderThreat || !ctx.buildingsUnderAttack.empty()) {

    } else if (ctx.total_units >= 4 && ctx.averageHealth > 0.65F) {

      ctx.state = AIState::Attacking;
    } else if (ctx.averageHealth > 0.80F) {

      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Retreating:

    if (ctx.stateTimer > 6.0F && ctx.averageHealth > 0.55F) {

      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Expanding:

    ctx.state = AIState::Idle;
    break;
  }

  if (ctx.state != previous_state) {
    ctx.stateTimer = 0.0F;
  }
}

} // namespace Game::Systems::AI
