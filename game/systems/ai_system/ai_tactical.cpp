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
    float min_force_ratio) -> TacticalUtils::EngagementAssessment {

  EngagementAssessment result;

  if (friendlies.empty() || enemies.empty()) {
    result.should_engage = false;
    return result;
  }

  result.friendly_count = static_cast<int>(friendlies.size());
  result.enemy_count = static_cast<int>(enemies.size());

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

  result.avg_friendly_health =
      valid_friendlies > 0 ? (total_friendly_health / valid_friendlies) : 1.0F;
  result.avg_enemy_health =
      valid_enemies > 0 ? (total_enemy_health / valid_enemies) : 1.0F;

  float const friendly_strength =
      static_cast<float>(result.friendly_count) * result.avg_friendly_health;
  float const enemy_strength =
      static_cast<float>(result.enemy_count) * result.avg_enemy_health;

  if (enemy_strength < 0.01F) {
    result.force_ratio = 10.0F;
  } else {
    result.force_ratio = friendly_strength / enemy_strength;
  }

  result.confidence_level =
      std::clamp((result.force_ratio - 0.5F) / 1.5F, 0.0F, 1.0F);

  result.should_engage = (result.force_ratio >= min_force_ratio);

  return result;
}

auto TacticalUtils::selectFocusFireTarget(
    const std::vector<const EntitySnapshot *> &,
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

    if (!enemy->is_building) {
      score += 5.0F;
    }

    if (currentTarget != 0 && enemy->id == currentTarget) {
      score += 10.0F;
    }

    bool const isolated = isTargetIsolated(*enemy, enemies, 8.0F);
    if (isolated) {
      score += 6.0F;
    }

    if (context.primary_barracks != 0) {
      float const dist_to_base =
          distance(enemy->posX, enemy->posY, enemy->posZ, context.base_pos_x,
                   context.base_pos_y, context.base_pos_z);

      if (dist_to_base < 16.0F) {
        score += (16.0F - dist_to_base) * 0.8F;
      }
    }

    if (context.state == AIState::Attacking && !enemy->is_building) {
      score += 3.0F;
    }

    if (score > best_target.score) {
      best_target.target_id = enemy->id;
      best_target.score = score;
      best_target.distance_to_group = dist;
      best_target.is_low_health =
          (enemy->max_health > 0 && enemy->health < enemy->max_health / 2);
      best_target.is_isolated = isolated;
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
    float isolation_radius) -> bool {

  const float isolation_radius_sq = isolation_radius * isolation_radius;
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

  auto spawn_type = Game::Units::spawn_typeFromString(unit_type);
  if (spawn_type && Game::Units::is_building_spawn(*spawn_type)) {
    return 0.5F;
  }

  if (unit_type == "base") {
    return 0.5F;
  }

  return 1.5F;
}

} // namespace Game::Systems::AI
