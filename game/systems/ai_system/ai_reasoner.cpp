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
  ctx.idle_units = 0;
  ctx.combat_units = 0;
  ctx.melee_count = 0;
  ctx.rangedCount = 0;
  ctx.damagedUnitsCount = 0;
  ctx.average_health = 1.0F;
  ctx.rally_x = 0.0F;
  ctx.rally_z = 0.0F;
  ctx.barracks_under_threat = false;
  ctx.nearby_threat_count = 0;
  ctx.closest_threat_distance = std::numeric_limits<float>::infinity();
  ctx.base_pos_x = 0.0F;
  ctx.base_pos_y = 0.0F;
  ctx.base_pos_z = 0.0F;
  ctx.visibleEnemyCount = 0;
  ctx.enemyBuildingsCount = 0;
  ctx.averageEnemyDistance = 0.0F;
  ctx.max_troops_per_player =
      Game::GameConfig::instance().get_max_troops_per_player();

  constexpr float attack_record_timeout = 10.0F;
  auto it = ctx.buildingsUnderAttack.begin();
  while (it != ctx.buildingsUnderAttack.end()) {
    bool still_exists = false;
    for (const auto &entity : snapshot.friendlies) {
      if (entity.id == it->first && entity.is_building) {
        still_exists = true;
        break;
      }
    }

    if (!still_exists ||
        (snapshot.game_time - it->second) > attack_record_timeout) {
      it = ctx.buildingsUnderAttack.erase(it);
    } else {
      ++it;
    }
  }

  float total_health_ratio = 0.0F;

  for (const auto &entity : snapshot.friendlies) {
    if (entity.is_building) {
      ctx.buildings.push_back(entity.id);

      if (entity.spawn_type == Game::Units::SpawnType::Barracks &&
          ctx.primaryBarracks == 0) {
        ctx.primaryBarracks = entity.id;
        ctx.rally_x = entity.posX - 5.0F;
        ctx.rally_z = entity.posZ;
        ctx.base_pos_x = entity.posX;
        ctx.base_pos_y = entity.posY;
        ctx.base_pos_z = entity.posZ;
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
          ctx.melee_count++;
        }
      }
    }

    if (!entity.movement.has_component || !entity.movement.has_target) {
      ctx.idle_units++;
    } else {
      ctx.combat_units++;
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

  ctx.average_health =
      (ctx.total_units > 0)
          ? (total_health_ratio / static_cast<float>(ctx.total_units))
          : 1.0F;

  ctx.visibleEnemyCount = static_cast<int>(snapshot.visibleEnemies.size());
  float total_enemy_dist = 0.0F;

  for (const auto &enemy : snapshot.visibleEnemies) {
    if (enemy.is_building) {
      ctx.enemyBuildingsCount++;
    }

    if (ctx.primaryBarracks != 0) {
      float const dist = distance(enemy.posX, enemy.posY, enemy.posZ,
                                  ctx.base_pos_x, ctx.base_pos_y, ctx.base_pos_z);
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
          distance_squared(enemy.posX, enemy.posY, enemy.posZ, ctx.base_pos_x,
                           ctx.base_pos_y, ctx.base_pos_z);

      if (dist_sq <= defend_radius_sq) {
        ctx.barracks_under_threat = true;
        ctx.nearby_threat_count++;

        float const dist = std::sqrt(std::max(dist_sq, 0.0F));
        ctx.closest_threat_distance = std::min(ctx.closest_threat_distance, dist);
      }
    }

    if (!ctx.barracks_under_threat) {
      ctx.closest_threat_distance = std::numeric_limits<float>::infinity();
    }
  }
}

void AIReasoner::updateStateMachine(AIContext &ctx, float delta_time) {
  ctx.state_timer += delta_time;
  ctx.decision_timer += delta_time;

  constexpr float min_state_duration = 3.0F;

  AIState previous_state = ctx.state;
  if ((ctx.barracks_under_threat || !ctx.buildingsUnderAttack.empty()) &&
      ctx.state != AIState::Defending) {

    ctx.state = AIState::Defending;
  }

  else if (ctx.visibleEnemyCount > 0 && ctx.averageEnemyDistance < 50.0F &&
           (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
    ctx.state = AIState::Defending;
  }

  if (ctx.decision_timer < 2.0F) {
    if (ctx.state != previous_state) {
      ctx.state_timer = 0.0F;
    }
    return;
  }

  ctx.decision_timer = 0.0F;
  previous_state = ctx.state;

  if (ctx.state_timer < min_state_duration &&
      ((!ctx.barracks_under_threat && ctx.buildingsUnderAttack.empty()) ||
       ctx.state == AIState::Defending)) {
    return;
  }

  switch (ctx.state) {
  case AIState::Idle:
    if (ctx.idle_units >= 2) {
      ctx.state = AIState::Gathering;
    } else if (ctx.average_health < 0.40F && ctx.total_units > 0) {

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
    } else if (ctx.average_health < 0.40F) {

      ctx.state = AIState::Defending;
    } else if (ctx.visibleEnemyCount > 0 && ctx.total_units >= 2) {

      ctx.state = AIState::Attacking;
    }
    break;

  case AIState::Attacking:
    if (ctx.average_health < 0.25F) {

      ctx.state = AIState::Retreating;
    } else if (ctx.total_units == 0) {

      ctx.state = AIState::Idle;
    }

    break;

  case AIState::Defending:

    if (ctx.barracks_under_threat || !ctx.buildingsUnderAttack.empty()) {

    } else if (ctx.total_units >= 4 && ctx.average_health > 0.65F) {

      ctx.state = AIState::Attacking;
    } else if (ctx.average_health > 0.80F) {

      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Retreating:

    if (ctx.state_timer > 6.0F && ctx.average_health > 0.55F) {

      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Expanding:

    ctx.state = AIState::Idle;
    break;
  }

  if (ctx.state != previous_state) {
    ctx.state_timer = 0.0F;
  }
}

} // namespace Game::Systems::AI
