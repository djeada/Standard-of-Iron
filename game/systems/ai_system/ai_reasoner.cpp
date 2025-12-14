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
    ctx.nation =
        Game::Systems::NationRegistry::instance().get_nation_for_player(
            ctx.player_id);
  }

  cleanupDeadUnits(snapshot, ctx);

  // Track unit count for deadlock detection
  int previous_unit_count = ctx.total_units;

  ctx.military_units.clear();
  ctx.buildings.clear();
  ctx.primary_barracks = 0;
  ctx.total_units = 0;
  ctx.idle_units = 0;
  ctx.combat_units = 0;
  ctx.melee_count = 0;
  ctx.ranged_count = 0;
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
  ctx.average_enemy_distance = 0.0F;
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
        if (ctx.nation->is_ranged_unit(troop_type)) {
          ctx.ranged_count++;
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

  // Deadlock detection: track if we're making progress
  if (ctx.total_units != previous_unit_count || ctx.combat_units > 0) {
    // Making progress - units changed or units are active
    ctx.consecutive_no_progress_cycles = 0;
    ctx.last_meaningful_action_time = snapshot.game_time;
  } else if (ctx.idle_units > 0 || ctx.visible_enemy_count > 0) {
    // Potentially stuck - have units but they're not doing anything productive
    ctx.consecutive_no_progress_cycles++;
  }
  
  ctx.last_total_units = ctx.total_units;
}

void AIReasoner::updateStateMachine(AIContext &ctx, float delta_time) {
  ctx.state_timer += delta_time;
  ctx.decision_timer += delta_time;

  constexpr float min_state_duration = 3.0F;
  constexpr int max_no_progress_cycles = 10; // ~3 seconds of no progress

  // Detect deadlock conditions
  bool deadlock_detected = false;
  
  // Force state change if stuck in same state too long
  if (ctx.state_timer > ctx.max_state_duration) {
    deadlock_detected = true;
  }
  
  // Detect if AI is stuck with idle units not making progress
  if (ctx.consecutive_no_progress_cycles >= max_no_progress_cycles && 
      ctx.idle_units > 0) {
    deadlock_detected = true;
  }

  AIState previous_state = ctx.state;
  
  // Critical override: immediate defense response
  if ((ctx.barracks_under_threat || !ctx.buildings_under_attack.empty()) &&
      ctx.state != AIState::Defending) {

    ctx.state = AIState::Defending;
  }

  else if (ctx.visible_enemy_count > 0 && ctx.average_enemy_distance < 50.0F &&
           (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
    ctx.state = AIState::Defending;
  }
  
  // Deadlock recovery: force a state transition
  if (deadlock_detected && ctx.state != AIState::Defending) {
    // Try to break out of stuck state
    if (ctx.state == AIState::Idle && ctx.total_units > 0) {
      ctx.state = AIState::Gathering;
    } else if (ctx.state == AIState::Gathering) {
      if (ctx.visible_enemy_count > 0) {
        ctx.state = AIState::Attacking;
      } else {
        ctx.state = AIState::Idle;
      }
    } else if (ctx.state == AIState::Attacking) {
      // Clear assignments to allow fresh decisions
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

  // Improved strategic decision-making: consider relative strength
  switch (ctx.state) {
  case AIState::Idle:
    if (ctx.idle_units >= 2) {
      ctx.state = AIState::Gathering;
    } else if (ctx.average_health < 0.40F && ctx.total_units > 0) {

      ctx.state = AIState::Defending;
    } else if (ctx.total_units >= 1 && ctx.visible_enemy_count > 0) {
      // Only attack if we have units, consider tactical situation
      if (ctx.total_units >= 2 || ctx.barracks_under_threat) {
        ctx.state = AIState::Attacking;
      }
    }
    break;

  case AIState::Gathering:
    if (ctx.total_units >= 3) {
      // Have enough units to attack
      ctx.state = AIState::Attacking;
    } else if (ctx.total_units < 2) {
      ctx.state = AIState::Idle;
    } else if (ctx.average_health < 0.40F) {
      // Army is weak, defend
      ctx.state = AIState::Defending;
    } else if (ctx.visible_enemy_count > 0 && ctx.total_units >= 2) {
      // Enemy spotted, engage with current force
      ctx.state = AIState::Attacking;
    }
    break;

  case AIState::Attacking:
    if (ctx.average_health < 0.25F) {
      // Army critically damaged, retreat
      ctx.state = AIState::Retreating;
    } else if (ctx.total_units == 0) {
      // Lost all units
      ctx.state = AIState::Idle;
    } else if (ctx.visible_enemy_count == 0 && ctx.state_timer > 15.0F) {
      // No enemies visible for a while, regroup
      ctx.state = AIState::Idle;
    } else if (ctx.average_health < 0.50F && ctx.damaged_units_count > ctx.total_units / 2) {
      // More than half damaged, consider retreating to defend
      if (!ctx.barracks_under_threat) {
        ctx.state = AIState::Defending;
      }
    }

    break;

  case AIState::Defending:

    if (ctx.barracks_under_threat || !ctx.buildings_under_attack.empty()) {
      // Stay in defense mode
    } else if (ctx.total_units >= 4 && ctx.average_health > 0.65F) {
      // Strong army, cleared threats, counter-attack
      ctx.state = AIState::Attacking;
    } else if (ctx.average_health > 0.80F && ctx.visible_enemy_count == 0) {
      // Healthy and no enemies, return to idle
      ctx.state = AIState::Idle;
    } else if (ctx.total_units < 2 && !ctx.barracks_under_threat) {
      // Very few units left, fall back to idle
      ctx.state = AIState::Idle;
    }
    break;

  case AIState::Retreating:

    if (ctx.state_timer > 6.0F && ctx.average_health > 0.55F) {
      // Recovered enough, switch to defense
      ctx.state = AIState::Defending;
    } else if (ctx.state_timer > 12.0F) {
      // Force recovery after 12 seconds
      ctx.state = AIState::Idle;
      ctx.assigned_units.clear();
    } else if (ctx.average_health > 0.70F && ctx.state_timer > 3.0F) {
      // Recovered quickly, can resume defense
      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Expanding:
    // Expansion not yet implemented, return to idle
    ctx.state = AIState::Idle;
    break;
  }

  if (ctx.state != previous_state) {
    ctx.state_timer = 0.0F;
    // Reset progress tracking on state change
    ctx.consecutive_no_progress_cycles = 0;
    ctx.debug_info.state_transitions++;
  }
}

void AIReasoner::validateState(AIContext &ctx) {
  // Ensure state is valid and consistent with reality
  
  // If no units, can only be Idle
  if (ctx.total_units == 0 && ctx.state != AIState::Idle) {
    ctx.state = AIState::Idle;
    ctx.state_timer = 0.0F;
    ctx.consecutive_no_progress_cycles = 0;
  }
  
  // If no barracks, should not be in certain states
  if (ctx.primary_barracks == 0 && ctx.buildings.empty()) {
    if (ctx.state == AIState::Defending && !ctx.barracks_under_threat) {
      ctx.state = AIState::Idle;
      ctx.state_timer = 0.0F;
    }
  }
  
  // Sanitize timers to prevent overflow
  if (ctx.state_timer > 1000.0F) {
    ctx.state_timer = ctx.max_state_duration;
  }
  if (ctx.decision_timer > 100.0F) {
    ctx.decision_timer = 0.0F;
  }
  
  // Clear stuck assignments for dead units
  if (ctx.assigned_units.size() > static_cast<size_t>(ctx.total_units * 2)) {
    // Too many assignments, likely has dead units
    ctx.assigned_units.clear();
  }
  
  // Reset consecutive no-progress if it gets too high
  if (ctx.consecutive_no_progress_cycles > 50) {
    ctx.consecutive_no_progress_cycles = 0;
    ctx.state = AIState::Idle;
    ctx.assigned_units.clear();
  }
}

} // namespace Game::Systems::AI
