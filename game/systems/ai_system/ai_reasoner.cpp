#include "ai_reasoner.h"
#include "../../core/ownership_constants.h"
#include "../../game_config.h"
#include "../nation_registry.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Systems::AI {

void AIReasoner::update_context(const AISnapshot &snapshot, AIContext &ctx) {

  if (ctx.nation == nullptr) {
    ctx.nation =
        Game::Systems::NationRegistry::instance().get_nation_for_player(
            ctx.player_id);
  }

  cleanup_dead_units(snapshot, ctx);

  int previous_unit_count = ctx.total_units;

  ctx.military_units.clear();
  ctx.buildings.clear();
  ctx.primary_barracks = 0;
  ctx.total_units = 0;
  ctx.idle_units = 0;
  ctx.combat_units = 0;
  ctx.melee_count = 0;
  ctx.ranged_count = 0;
  ctx.builder_count = 0;
  ctx.damaged_units_count = 0;
  ctx.average_health = 1.0F;
  ctx.rally_x = 0.0F;
  ctx.rally_z = 0.0F;
  ctx.barracks_under_threat = false;
  ctx.nearby_threat_count = 0;
  ctx.closest_threat_distance = std::numeric_limits<float>::infinity();
  ctx.base_pos_x = 0.0F;
  ctx.base_pos_y = 0.0F;
  ctx.base_pos_z = 0.0F;
  ctx.visible_enemy_count = 0;
  ctx.enemy_buildings_count = 0;
  ctx.neutral_barracks_count = 0;
  ctx.average_enemy_distance = 0.0F;
  ctx.home_count = 0;
  ctx.defense_tower_count = 0;
  ctx.barracks_count = 0;
  ctx.max_troops_per_player =
      Game::GameConfig::instance().get_max_troops_per_player();

  constexpr float attack_record_timeout = 10.0F;
  auto it = ctx.buildings_under_attack.begin();
  while (it != ctx.buildings_under_attack.end()) {
    bool still_exists = false;
    for (const auto &entity : snapshot.friendly_units) {
      if (entity.id == it->first && entity.is_building) {
        still_exists = true;
        break;
      }
    }

    if (!still_exists ||
        (snapshot.game_time - it->second) > attack_record_timeout) {
      it = ctx.buildings_under_attack.erase(it);
    } else {
      ++it;
    }
  }

  float total_health_ratio = 0.0F;

  for (const auto &entity : snapshot.friendly_units) {
    if (entity.is_building) {
      ctx.buildings.push_back(entity.id);

      if (entity.spawn_type == Game::Units::SpawnType::Home) {
        ctx.home_count++;
      } else if (entity.spawn_type == Game::Units::SpawnType::DefenseTower) {
        ctx.defense_tower_count++;
      } else if (entity.spawn_type == Game::Units::SpawnType::Barracks) {
        ctx.barracks_count++;
      }

      if (entity.spawn_type == Game::Units::SpawnType::Barracks &&
          ctx.primary_barracks == 0) {
        ctx.primary_barracks = entity.id;
        ctx.rally_x = entity.posX - 5.0F;
        ctx.rally_z = entity.posZ;
        ctx.base_pos_x = entity.posX;
        ctx.base_pos_y = entity.posY;
        ctx.base_pos_z = entity.posZ;
      }
      continue;
    }

    ctx.military_units.push_back(entity.id);
    ctx.total_units++;

    if (ctx.nation != nullptr) {
      auto troop_type_opt =
          Game::Units::spawn_typeToTroopType(entity.spawn_type);
      if (troop_type_opt) {
        auto troop_type = *troop_type_opt;
        if (troop_type == Game::Units::TroopType::Builder) {
          ctx.builder_count++;
        } else if (ctx.nation->is_ranged_unit(troop_type)) {
          ctx.ranged_count++;
        } else if (ctx.nation->is_melee_unit(troop_type)) {
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
        ctx.damaged_units_count++;
      }
    }
  }

  ctx.average_health =
      (ctx.total_units > 0)
          ? (total_health_ratio / static_cast<float>(ctx.total_units))
          : 1.0F;

  ctx.visible_enemy_count = static_cast<int>(snapshot.visible_enemies.size());
  float total_enemy_dist = 0.0F;

  for (const auto &enemy : snapshot.visible_enemies) {
    if (enemy.is_building) {
      ctx.enemy_buildings_count++;

      if (enemy.spawn_type == Game::Units::SpawnType::Barracks &&
          Game::Core::isNeutralOwner(enemy.owner_id)) {
        ctx.neutral_barracks_count++;
      }
    }

    if (ctx.primary_barracks != 0) {
      float const dist =
          distance(enemy.posX, enemy.posY, enemy.posZ, ctx.base_pos_x,
                   ctx.base_pos_y, ctx.base_pos_z);
      total_enemy_dist += dist;
    }
  }

  ctx.average_enemy_distance =
      (ctx.visible_enemy_count > 0)
          ? (total_enemy_dist / static_cast<float>(ctx.visible_enemy_count))
          : 1000.0F;

  if (ctx.primary_barracks != 0) {

    constexpr float defend_radius = 40.0F;
    constexpr float critical_radius = 20.0F;
    const float defend_radius_sq = defend_radius * defend_radius;
    const float critical_radius_sq = critical_radius * critical_radius;

    for (const auto &enemy : snapshot.visible_enemies) {
      float const dist_sq =
          distance_squared(enemy.posX, enemy.posY, enemy.posZ, ctx.base_pos_x,
                           ctx.base_pos_y, ctx.base_pos_z);

      if (dist_sq <= defend_radius_sq) {
        ctx.barracks_under_threat = true;
        ctx.nearby_threat_count++;

        float const dist = std::sqrt(std::max(dist_sq, 0.0F));
        ctx.closest_threat_distance =
            std::min(ctx.closest_threat_distance, dist);
      }
    }

    if (!ctx.barracks_under_threat) {
      ctx.closest_threat_distance = std::numeric_limits<float>::infinity();
    }
  }

  if (ctx.total_units != previous_unit_count || ctx.combat_units > 0) {

    ctx.consecutive_no_progress_cycles = 0;
    ctx.last_meaningful_action_time = snapshot.game_time;
  } else if (ctx.idle_units > 0 || ctx.visible_enemy_count > 0) {

    ctx.consecutive_no_progress_cycles++;
  }

  if (ctx.last_meaningful_action_time == 0.0F) {
    ctx.last_meaningful_action_time = snapshot.game_time;
  }

  ctx.last_total_units = ctx.total_units;
}

void AIReasoner::update_state_machine(const AISnapshot &snapshot,
                                      AIContext &ctx, float delta_time) {
  ctx.state_timer += delta_time;
  ctx.decision_timer += delta_time;

  constexpr float min_state_duration = 3.0F;
  constexpr float max_no_progress_duration = 3.0F;

  bool deadlock_detected = false;

  if (ctx.state_timer > ctx.max_state_duration) {
    deadlock_detected = true;
  }

  float time_since_progress =
      snapshot.game_time - ctx.last_meaningful_action_time;
  if (time_since_progress >= max_no_progress_duration && ctx.idle_units > 0) {
    deadlock_detected = true;
  }

  AIState previous_state = ctx.state;

  if ((ctx.barracks_under_threat || !ctx.buildings_under_attack.empty()) &&
      ctx.state != AIState::Defending) {

    ctx.state = AIState::Defending;
  }

  else if (ctx.visible_enemy_count > 0 && ctx.average_enemy_distance < 50.0F &&
           (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
    ctx.state = AIState::Defending;
  }

  if (deadlock_detected && ctx.state != AIState::Defending) {

    if (ctx.state == AIState::Idle && ctx.total_units > 0) {
      ctx.state = AIState::Gathering;
    } else if (ctx.state == AIState::Gathering) {
      if (ctx.visible_enemy_count > 0) {
        ctx.state = AIState::Attacking;
      } else {
        ctx.state = AIState::Idle;
      }
    } else if (ctx.state == AIState::Attacking) {

      ctx.assigned_units.clear();
      if (ctx.average_health < 0.5F) {
        ctx.state = AIState::Defending;
      } else {
        ctx.state = AIState::Idle;
      }
    }
    ctx.consecutive_no_progress_cycles = 0;
    ctx.debug_info.deadlock_recoveries++;
  }

  if (ctx.decision_timer < 2.0F) {
    if (ctx.state != previous_state) {
      ctx.state_timer = 0.0F;
      ctx.debug_info.state_transitions++;
    }
    return;
  }

  ctx.decision_timer = 0.0F;
  ctx.debug_info.total_decisions_made++;
  previous_state = ctx.state;

  if (ctx.state_timer < min_state_duration &&
      ((!ctx.barracks_under_threat && ctx.buildings_under_attack.empty()) ||
       ctx.state == AIState::Defending)) {
    return;
  }

  switch (ctx.state) {
  case AIState::Idle:
    if (ctx.idle_units >= 1) {

      ctx.state = AIState::Gathering;
    } else if (ctx.average_health < 0.40F && ctx.total_units > 0) {

      ctx.state = AIState::Defending;
    } else if (ctx.neutral_barracks_count > 0 && ctx.total_units >= 3) {

      ctx.state = AIState::Expanding;
    } else if (ctx.total_units >= 1 && ctx.visible_enemy_count > 0) {

      if (ctx.total_units >= 2 || ctx.barracks_under_threat) {
        ctx.state = AIState::Attacking;
      }
    }
    break;

  case AIState::Gathering: {

    constexpr int MIN_UNITS_FOR_REACTIVE_ATTACK = 2;
    constexpr int MIN_UNITS_FOR_PROACTIVE_ATTACK = 4;
    constexpr int MIN_UNITS_FOR_EXPANSION = 3;

    if (ctx.total_units < 1) {
      ctx.state = AIState::Idle;
    } else if (ctx.average_health < 0.40F) {

      ctx.state = AIState::Defending;
    } else if (ctx.neutral_barracks_count > 0 &&
               ctx.total_units >= MIN_UNITS_FOR_EXPANSION) {

      ctx.state = AIState::Expanding;
    } else if (ctx.visible_enemy_count > 0 &&
               ctx.total_units >= MIN_UNITS_FOR_REACTIVE_ATTACK) {

      ctx.state = AIState::Attacking;
    } else if (ctx.total_units >= MIN_UNITS_FOR_PROACTIVE_ATTACK) {

      ctx.state = AIState::Attacking;
    }
  } break;

  case AIState::Attacking:
    if (ctx.average_health < 0.25F) {

      ctx.state = AIState::Retreating;
    } else if (ctx.total_units == 0) {

      ctx.state = AIState::Idle;
    } else if (ctx.visible_enemy_count == 0 && ctx.state_timer > 15.0F) {

      ctx.state = AIState::Idle;
    } else if (ctx.average_health < 0.50F &&
               ctx.damaged_units_count * 2 > ctx.total_units) {

      if (!ctx.barracks_under_threat) {
        ctx.state = AIState::Defending;
      }
    }

    break;

  case AIState::Defending:

    if (ctx.barracks_under_threat || !ctx.buildings_under_attack.empty()) {

    } else if (ctx.total_units >= 4 && ctx.average_health > 0.65F) {

      ctx.state = AIState::Attacking;
    } else if (ctx.average_health > 0.80F && ctx.visible_enemy_count == 0) {

      ctx.state = AIState::Idle;
    } else if (ctx.total_units < 2 && !ctx.barracks_under_threat) {

      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Retreating:

    if (ctx.state_timer > 6.0F && ctx.average_health > 0.55F) {

      ctx.state = AIState::Defending;
    } else if (ctx.state_timer > 12.0F) {

      ctx.state = AIState::Idle;
      ctx.assigned_units.clear();
    } else if (ctx.average_health > 0.70F && ctx.state_timer > 3.0F) {

      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Expanding:

    if (ctx.neutral_barracks_count == 0) {

      if (ctx.visible_enemy_count > 0) {
        ctx.state = AIState::Attacking;
      } else {
        ctx.state = AIState::Gathering;
      }
    } else if (ctx.total_units < 2) {

      ctx.state = AIState::Gathering;
    } else if (ctx.barracks_under_threat ||
               !ctx.buildings_under_attack.empty()) {

      ctx.state = AIState::Defending;
    } else if (ctx.average_health < 0.40F) {

      ctx.state = AIState::Defending;
    }
    break;
  }

  if (ctx.state != previous_state) {
    ctx.state_timer = 0.0F;

    ctx.consecutive_no_progress_cycles = 0;
    ctx.debug_info.state_transitions++;
  }
}

void AIReasoner::validate_state(AIContext &ctx) {

  constexpr size_t MAX_ASSIGNMENT_MULTIPLIER = 2;
  constexpr int MAX_NO_PROGRESS_CYCLES = 50;
  constexpr float MAX_STATE_TIMER = 1000.0F;
  constexpr float MAX_DECISION_TIMER = 100.0F;

  if (ctx.total_units == 0 && ctx.state != AIState::Idle) {
    ctx.state = AIState::Idle;
    ctx.state_timer = 0.0F;
    ctx.consecutive_no_progress_cycles = 0;
  }

  if (ctx.primary_barracks == 0 && ctx.buildings.empty()) {
    if (ctx.state == AIState::Defending && !ctx.barracks_under_threat) {
      ctx.state = AIState::Idle;
      ctx.state_timer = 0.0F;
    }
  }

  if (ctx.state_timer > MAX_STATE_TIMER) {
    ctx.state_timer = ctx.max_state_duration;
  }
  if (ctx.decision_timer > MAX_DECISION_TIMER) {
    ctx.decision_timer = 0.0F;
  }

  size_t max_expected_assignments =
      static_cast<size_t>(ctx.total_units) * MAX_ASSIGNMENT_MULTIPLIER;
  if (ctx.assigned_units.size() > max_expected_assignments) {

    ctx.assigned_units.clear();
  }

  if (ctx.consecutive_no_progress_cycles > MAX_NO_PROGRESS_CYCLES) {
    ctx.consecutive_no_progress_cycles = 0;
    ctx.state = AIState::Idle;
    ctx.assigned_units.clear();
  }
}

} // namespace Game::Systems::AI
