#include "ai_tactical.h"
#include "../../units/troop_type.h"
#include "../nation_registry.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace Game::Systems::AI {

auto TacticalUtils::assessEngagement(
    const std::vector<const EntitySnapshot *> &friendlies,
    const std::vector<const ContactSnapshot *> &enemies,
    float minForceRatio) -> TacticalUtils::EngagementAssessment {

  EngagementAssessment result;

  if (friendlies.empty() || enemies.empty()) {
    result.shouldEngage = false;
    return result;
  }

  result.friendlyCount = static_cast<int>(friendlies.size());
  result.enemyCount = static_cast<int>(enemies.size());

  float total_friendly_health = 0.0F;
  float total_enemy_health = 0.0F;
  int valid_friendlies = 0;
  int valid_enemies = 0;

  for (const auto *unit : friendlies) {
    if (unit->max_health > 0) {
      total_friendly_health += static_cast<float>(unit->health) /
                               static_cast<float>(unit->max_health);
      ++valid_friendlies;
    }
  }

  for (const auto *enemy : enemies) {
    if (enemy->max_health > 0) {
      total_enemy_health += static_cast<float>(enemy->health) /
                            static_cast<float>(enemy->max_health);
      ++valid_enemies;
    }
  }

  result.avgFriendlyHealth =
      valid_friendlies > 0 ? (total_friendly_health / valid_friendlies) : 1.0F;
  result.avgEnemyHealth =
      valid_enemies > 0 ? (total_enemy_health / valid_enemies) : 1.0F;

  float const friendly_strength =
      static_cast<float>(result.friendlyCount) * result.avgFriendlyHealth;
  float const enemy_strength =
      static_cast<float>(result.enemyCount) * result.avgEnemyHealth;

  if (enemy_strength < 0.01F) {
    result.forceRatio = 10.0F;
  } else {
    result.forceRatio = friendly_strength / enemy_strength;
  }

  result.confidenceLevel =
      std::clamp((result.forceRatio - 0.5F) / 1.5F, 0.0F, 1.0F);

  result.shouldEngage = (result.forceRatio >= minForceRatio);

  return result;
}

auto TacticalUtils::selectFocusFireTarget(
    const std::vector<const EntitySnapshot *> &attackers,
    const std::vector<const ContactSnapshot *> &enemies, float group_center_x,
    float group_center_y, float group_center_z, const AIContext &context,
    Engine::Core::EntityID currentTarget) -> TacticalUtils::TargetScore {

  TargetScore best_target;
  best_target.score = -std::numeric_limits<float>::infinity();

  if (enemies.empty()) {
    return best_target;
  }

  for (const auto *enemy : enemies) {
    float score = 0.0F;

    float const dist = distance(enemy->posX, enemy->posY, enemy->posZ,
                                group_center_x, group_center_y, group_center_z);
    score -= dist * 0.5F;

    if (enemy->max_health > 0) {
      float const health_ratio = static_cast<float>(enemy->health) /
                                 static_cast<float>(enemy->max_health);

      if (health_ratio < 0.5F) {
        score += 8.0F * (1.0F - health_ratio);
      }

      if (health_ratio < 0.25F) {
        score += 12.0F;
      }
    }

    float const type_priority = getUnitTypePriority(
        Game::Units::spawn_typeToString(enemy->spawn_type), context.nation);
    score += type_priority * 3.0F;

    if (!enemy->isBuilding) {
      score += 5.0F;
    }

    if (currentTarget != 0 && enemy->id == currentTarget) {
      score += 10.0F;
    }

    bool const isolated = isTargetIsolated(*enemy, enemies, 8.0F);
    if (isolated) {
      score += 6.0F;
    }

    if (context.primaryBarracks != 0) {
      float const dist_to_base =
          distance(enemy->posX, enemy->posY, enemy->posZ, context.basePosX,
                   context.basePosY, context.basePosZ);

      if (dist_to_base < 16.0F) {
        score += (16.0F - dist_to_base) * 0.8F;
      }
    }

    if (context.state == AIState::Attacking && !enemy->isBuilding) {
      score += 3.0F;
    }

    if (score > best_target.score) {
      best_target.target_id = enemy->id;
      best_target.score = score;
      best_target.distanceToGroup = dist;
      best_target.isLowHealth =
          (enemy->max_health > 0 && enemy->health < enemy->max_health / 2);
      best_target.isIsolated = isolated;
    }
  }

  return best_target;
}

auto TacticalUtils::calculateForceStrength(
    const std::vector<const EntitySnapshot *> &units) -> float {

  float strength = 0.0F;
  for (const auto *unit : units) {
    if (unit->max_health > 0) {
      float const health_ratio = static_cast<float>(unit->health) /
                                 static_cast<float>(unit->max_health);
      strength += health_ratio;
    } else {
      strength += 1.0F;
    }
  }
  return strength;
}

auto TacticalUtils::calculateForceStrength(
    const std::vector<const ContactSnapshot *> &units) -> float {

  float strength = 0.0F;
  for (const auto *unit : units) {
    if (unit->max_health > 0) {
      float const health_ratio = static_cast<float>(unit->health) /
                                 static_cast<float>(unit->max_health);
      strength += health_ratio;
    } else {
      strength += 1.0F;
    }
  }
  return strength;
}

auto TacticalUtils::isTargetIsolated(
    const ContactSnapshot &target,
    const std::vector<const ContactSnapshot *> &allEnemies,
    float isolationRadius) -> bool {

  const float isolation_radius_sq = isolationRadius * isolationRadius;
  int nearby_allies = 0;

  for (const auto *enemy : allEnemies) {

    if (enemy->id == target.id) {
      continue;
    }

    float const dist_sq =
        distance_squared(target.posX, target.posY, target.posZ, enemy->posX,
                         enemy->posY, enemy->posZ);

    if (dist_sq <= isolation_radius_sq) {
      ++nearby_allies;
    }
  }

  return (nearby_allies <= 1);
}

auto TacticalUtils::getUnitTypePriority(const std::string &unit_type,
                                        const Game::Systems::Nation *nation)
    -> float {

  if (nation != nullptr) {
    auto troop_type = Game::Units::troop_typeFromString(unit_type);
    if (nation->is_ranged_unit(troop_type)) {
      return 3.0F;
    }
    if (nation->isMeleeUnit(troop_type)) {
      return 2.0F;
    }
  }

  if (unit_type == "worker" || unit_type == "villager") {
    return 1.0F;
  }

  if (unit_type == "barracks" || unit_type == "base") {
    return 0.5F;
  }

  return 1.5F;
}

} // namespace Game::Systems::AI
