#include "ai_reasoner.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

#include "ai_attack_wave.h"
#include "ai_base_manager.h"
#include "ai_context_census.h"
#include "ai_doctrine_rules.h"
#include "ai_expansion_planner.h"
#include "ai_force_assignment.h"
#include "ai_macro_targets.h"
#include "ai_settlement_frame.h"
#include "ai_stall_recovery.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

struct AnchorCandidate {
  float x = 0.0F;
  float z = 0.0F;
};

constexpr float k_anchor_cluster_radius = 12.0F;

auto densest_anchor_cluster(const std::vector<AnchorCandidate>& candidates)
    -> std::optional<AnchorCandidate> {
  if (candidates.empty()) {
    return std::nullopt;
  }

  const float cluster_radius_sq = k_anchor_cluster_radius * k_anchor_cluster_radius;
  int best_count = -1;
  float best_distance_sum = std::numeric_limits<float>::infinity();
  AnchorCandidate best_center{};

  for (const auto& candidate : candidates) {
    int cluster_count = 0;
    float sum_x = 0.0F;
    float sum_z = 0.0F;
    float distance_sum = 0.0F;

    for (const auto& other : candidates) {
      const float dx = other.x - candidate.x;
      const float dz = other.z - candidate.z;
      const float distance_sq = dx * dx + dz * dz;
      if (distance_sq > cluster_radius_sq) {
        continue;
      }

      cluster_count++;
      sum_x += other.x;
      sum_z += other.z;
      distance_sum += distance_sq;
    }

    if (cluster_count <= 0) {
      continue;
    }

    if (cluster_count > best_count ||
        (cluster_count == best_count && distance_sum < best_distance_sum)) {
      best_count = cluster_count;
      best_distance_sum = distance_sum;
      const float scale = 1.0F / static_cast<float>(cluster_count);
      best_center = {sum_x * scale, sum_z * scale};
    }
  }

  if (best_count <= 0) {
    return std::nullopt;
  }
  return best_center;
}

auto select_defense_anchor(const AISnapshot& snapshot)
    -> std::optional<AnchorCandidate> {
  if (snapshot.defense_anchors.empty()) {
    return std::nullopt;
  }

  int best_score = -1;
  float best_distance_sum = std::numeric_limits<float>::infinity();
  AnchorCandidate best_anchor{snapshot.defense_anchors.front().pos_x,
                              snapshot.defense_anchors.front().pos_z};
  constexpr float k_anchor_unit_radius_sq = 18.0F * 18.0F;

  for (const auto& anchor : snapshot.defense_anchors) {
    int score = 0;
    float distance_sum = 0.0F;
    for (const auto& entity : snapshot.friendly_units) {
      if (entity.is_building || entity.spawn_type == Game::Units::SpawnType::Builder) {
        continue;
      }
      const float dist_sq = distance_squared(
          entity.pos_x, entity.pos_y, entity.pos_z, anchor.pos_x, 0.0F, anchor.pos_z);
      if (dist_sq <= k_anchor_unit_radius_sq) {
        ++score;
        distance_sum += dist_sq;
      }
    }

    if (score > best_score ||
        (score == best_score && distance_sum < best_distance_sum)) {
      best_score = score;
      best_distance_sum = distance_sum;
      best_anchor = {anchor.pos_x, anchor.pos_z};
    }
  }

  return best_anchor;
}

void adopt_primary_barracks(AIContext& ctx, const EntitySnapshot& barracks) {
  ctx.primary_barracks = barracks.id;
  ctx.anchor_station.offered = true;
  ctx.anchor_station.x = barracks.pos_x - 5.0F;
  ctx.anchor_station.z = barracks.pos_z;
  ctx.base_pos_x = barracks.pos_x;
  ctx.base_pos_y = barracks.pos_y;
  ctx.base_pos_z = barracks.pos_z;
  ctx.has_base_anchor = true;
  ctx.anchor_is_structural = true;
}

void adopt_defense_anchor(AIContext& ctx, const AnchorCandidate& anchor) {
  ctx.base_pos_x = anchor.x;
  ctx.base_pos_y = 0.0F;
  ctx.base_pos_z = anchor.z;
  ctx.anchor_station.offered = true;
  ctx.anchor_station.x = anchor.x;
  ctx.anchor_station.z = anchor.z;
  ctx.has_base_anchor = true;
  ctx.anchor_is_structural = true;
}

void adopt_army_anchor(const AISnapshot& snapshot, AIContext& ctx) {
  std::vector<AnchorCandidate> unit_positions;
  unit_positions.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.spawn_type == Game::Units::SpawnType::Builder) {
      continue;
    }
    unit_positions.push_back({entity.pos_x, entity.pos_z});
  }

  if (const auto anchor = densest_anchor_cluster(unit_positions); anchor.has_value()) {
    ctx.base_pos_x = anchor->x;
    ctx.base_pos_y = 0.0F;
    ctx.base_pos_z = anchor->z;
    ctx.anchor_station.offered = true;
    ctx.anchor_station.x = anchor->x - 5.0F;
    ctx.anchor_station.z = anchor->z;
    ctx.has_base_anchor = true;
  }
}

void resolve_base_anchor(const AISnapshot& snapshot,
                         AIContext& ctx,
                         const EntitySnapshot* primary_barracks) {
  if (primary_barracks != nullptr) {
    adopt_primary_barracks(ctx, *primary_barracks);
  }

  if (!ctx.has_base_anchor && is_no_economy_nation(ctx)) {
    if (const auto anchor = select_defense_anchor(snapshot); anchor.has_value()) {
      adopt_defense_anchor(ctx, *anchor);
    }
  }

  if (!ctx.has_base_anchor) {
    adopt_army_anchor(snapshot, ctx);
  }
}

void plan_force(const AISnapshot& snapshot, AIContext& ctx) {
  ctx.macro_targets =
      compute_macro_targets(snapshot, ctx, count_siege_engines(snapshot));
  update_assault_unit_ids(snapshot, ctx);
  update_reserve_unit_ids(snapshot, ctx);
  update_harass_unit_ids(snapshot, ctx);

  update_attack_wave(snapshot, ctx);
  ctx.assembled_unit_count = committed_army_count(snapshot, ctx);
}

void track_progress(const AISnapshot& snapshot,
                    AIContext& ctx,
                    int previous_unit_count) {
  if (has_active_local_threat(ctx)) {
    ctx.last_local_threat_time = snapshot.game_time;
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
}

void step_idle(AIContext& ctx, bool no_economy_nation) {
  if (ctx.idle_units >= 1) {
    ctx.state = AIState::Gathering;
  } else if (ctx.average_health < (0.40F * ctx.strategy_config.defense_modifier) &&
             ctx.total_units > 0) {
    ctx.state = AIState::Defending;
  } else if (!no_economy_nation && wants_expansion(ctx)) {
    ctx.state = AIState::Expanding;
  } else if (ctx.total_units >= 1 && ctx.visible_enemy_count > 0) {
    if (!no_economy_nation && can_initiate_attack(ctx.strategy_config) &&
        committable_attack_force(ctx) >= reactive_attack_size(ctx.strategy_config)) {
      ctx.state = AIState::Attacking;
    }
  }
}

void step_gathering(AIContext& ctx, bool no_economy_nation) {
  const auto& strategy = ctx.strategy_config;

  const int min_units_for_reactive_attack = reactive_attack_size(strategy);
  const int min_units_for_proactive_attack = proactive_attack_size(strategy);
  if (ctx.total_units < 1) {
    ctx.state = AIState::Idle;
  } else if (ctx.average_health < (0.40F * strategy.defense_modifier)) {
    ctx.state = AIState::Defending;
  } else if (!no_economy_nation && wants_expansion(ctx)) {
    ctx.state = AIState::Expanding;
  } else if (!no_economy_nation && ctx.visible_enemy_count > 0 &&
             can_initiate_attack(strategy) &&
             committable_attack_force(ctx) >= min_units_for_reactive_attack) {
    ctx.state = AIState::Attacking;
  } else if (!no_economy_nation &&
             committable_attack_force(ctx) >=
                 std::max(min_units_for_proactive_attack,
                          ctx.macro_targets.assembly_size) &&
             can_initiate_attack(strategy)) {
    ctx.state = AIState::Attacking;
  }
}

void step_attacking(AIContext& ctx, bool no_economy_nation) {
  if (no_economy_nation) {
    ctx.state = has_active_local_threat(ctx) ? AIState::Defending : AIState::Gathering;
    return;
  }
  if (ctx.average_health < ctx.strategy_config.retreat_threshold) {
    ctx.state = AIState::Retreating;
  } else if (ctx.total_units == 0) {
    ctx.state = AIState::Idle;
  } else if (ctx.visible_enemy_count == 0 && ctx.state_timer > 15.0F) {
    ctx.state = AIState::Idle;
  } else if (ctx.average_health < (0.50F * ctx.strategy_config.defense_modifier) &&
             ctx.damaged_units_count * 2 > ctx.total_units) {
    if (!ctx.barracks_under_threat) {
      ctx.state = AIState::Defending;
    }
  }
}

void step_defending(const AISnapshot& snapshot,
                    AIContext& ctx,
                    bool no_economy_nation) {
  if (has_recent_local_threat(ctx, snapshot.game_time)) {
    return;
  }
  if (!no_economy_nation && can_initiate_attack(ctx.strategy_config) &&
      committable_attack_force(ctx) >=
          std::max(proactive_attack_size(ctx.strategy_config),
                   ctx.macro_targets.assembly_size) &&
      ctx.average_health > resume_attack_health_threshold(ctx.strategy_config)) {
    ctx.state = AIState::Attacking;
  } else if (ctx.total_units < 2) {
    ctx.state = AIState::Idle;
  } else if (!no_economy_nation && ctx.visible_enemy_count > 0) {
    ctx.state = AIState::Gathering;
  } else if (ctx.average_health >
             return_to_idle_health_threshold(ctx.strategy_config)) {
    ctx.state = no_economy_nation ? AIState::Gathering : AIState::Idle;
  } else {
    ctx.state = AIState::Gathering;
  }
}

void step_retreating(const AISnapshot& snapshot,
                     AIContext& ctx,
                     bool no_economy_nation) {
  if (no_economy_nation && !has_recent_local_threat(ctx, snapshot.game_time)) {
    ctx.state = AIState::Gathering;
    return;
  }

  if (ctx.state_timer > 6.0F && ctx.average_health > 0.55F) {
    ctx.state = AIState::Defending;
  } else if (ctx.state_timer > 12.0F) {
    ctx.state = AIState::Idle;
    ctx.assigned_units.clear();
  } else if (ctx.average_health > 0.70F && ctx.state_timer > 3.0F) {
    ctx.state = AIState::Defending;
  }
}

void step_expanding(AIContext& ctx, bool no_economy_nation) {
  if (no_economy_nation) {
    ctx.state = has_active_local_threat(ctx) ? AIState::Defending : AIState::Gathering;
    return;
  }

  if (!wants_expansion(ctx)) {
    ctx.state = ctx.visible_enemy_count > 0 ? AIState::Attacking : AIState::Gathering;
  } else if (ctx.total_units < 2) {
    ctx.state = AIState::Gathering;
  } else if (ctx.barracks_under_threat || !ctx.buildings_under_attack.empty()) {
    ctx.state = AIState::Defending;
  } else if (ctx.average_health < 0.40F) {
    ctx.state = AIState::Defending;
  }
}

void react_to_threat(AIContext& ctx) {
  if (has_active_local_threat(ctx) && ctx.state != AIState::Defending) {
    ctx.state = AIState::Defending;
  } else if ((ctx.nearby_threat_count > 0) &&
             (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
    ctx.state = AIState::Defending;
  }
}

void break_deadlock(AIContext& ctx, bool no_economy_nation) {
  if (ctx.state == AIState::Idle && ctx.total_units > 0) {
    ctx.state = AIState::Gathering;
  } else if (ctx.state == AIState::Gathering) {
    if (ctx.visible_enemy_count > 0 && can_initiate_attack(ctx.strategy_config) &&
        !no_economy_nation &&
        committable_attack_force(ctx) >= reactive_attack_size(ctx.strategy_config)) {
      ctx.state = AIState::Attacking;
    } else if (ctx.visible_enemy_count == 0) {
      ctx.state = AIState::Idle;
    }
  } else if (ctx.state == AIState::Attacking) {
    ctx.assigned_units.clear();
    ctx.state = ctx.average_health < 0.5F ? AIState::Defending : AIState::Idle;
  }
  ctx.consecutive_no_progress_cycles = 0;
}

void note_state_change(AIContext& ctx, AIState previous_state) {
  ctx.state_timer = 0.0F;
  if (previous_state == AIState::Defending && ctx.state != AIState::Defending) {
    ctx.assigned_units.clear();
  }
  if (ctx.state == AIState::Defending || ctx.state == AIState::Retreating) {
    release_units(ctx.harass_unit_ids, ctx);
  }
  ctx.consecutive_no_progress_cycles = 0;
}

} // namespace

void AIReasoner::update_context(const AISnapshot& snapshot, AIContext& ctx) {
  ctx.nation = snapshot.nation.get();

  const auto alive_ids = cleanup_dead_units(snapshot, ctx, snapshot.game_time);

  int const previous_unit_count = ctx.total_units;
  const Engine::Core::EntityID previous_primary_barracks = ctx.primary_barracks;
  const bool had_previous_site = ctx.has_expansion_site;
  const float previous_site_x = ctx.expansion_site_x;
  const float previous_site_z = ctx.expansion_site_z;

  reset_context_counters(snapshot, ctx);
  expire_attack_records(snapshot, ctx, alive_ids);
  const FriendlyTally tally =
      tally_friendly_units(snapshot, ctx, previous_primary_barracks);

  resolve_base_anchor(snapshot, ctx, tally.primary_barracks);
  update_expansion_site(
      snapshot, ctx, had_previous_site, previous_site_x, previous_site_z);

  AIBaseManager::update(snapshot, ctx);
  apply_settlement_stations(snapshot, ctx);
  resolve_station(snapshot, ctx);
  update_station_report(snapshot, ctx);

  plan_force(snapshot, ctx);

  ctx.average_health =
      (ctx.total_units > 0)
          ? (tally.total_health_ratio / static_cast<float>(ctx.total_units))
          : 1.0F;
  ctx.visible_enemy_count = static_cast<int>(snapshot.visible_enemies.size());
  count_neutral_barracks(snapshot, ctx);
  count_nearby_threats(snapshot, ctx);
  track_progress(snapshot, ctx, previous_unit_count);
}

void AIReasoner::update_state_machine(const AISnapshot& snapshot,
                                      AIContext& ctx,
                                      float delta_time) {
  ctx.state_timer += delta_time;
  ctx.decision_timer += delta_time;

  constexpr float min_state_duration = 3.0F;
  constexpr float max_no_progress_duration = 3.0F;

  const bool no_economy_nation = is_no_economy_nation(ctx);

  bool deadlock_detected = ctx.state_timer > ctx.max_state_duration;
  float const time_since_progress =
      snapshot.game_time - ctx.last_meaningful_action_time;
  if (time_since_progress >= max_no_progress_duration && ctx.idle_units > 0) {
    deadlock_detected = true;
  }

  AIState previous_state = ctx.state;

  react_to_threat(ctx);

  if (deadlock_detected && ctx.state != AIState::Defending) {
    break_deadlock(ctx, no_economy_nation);
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
      ((!has_active_local_threat(ctx)) || ctx.state == AIState::Defending)) {
    return;
  }

  switch (ctx.state) {
  case AIState::Idle:
    step_idle(ctx, no_economy_nation);
    break;
  case AIState::Gathering:
    step_gathering(ctx, no_economy_nation);
    break;
  case AIState::Attacking:
    step_attacking(ctx, no_economy_nation);
    break;
  case AIState::Defending:
    step_defending(snapshot, ctx, no_economy_nation);
    break;
  case AIState::Retreating:
    step_retreating(snapshot, ctx, no_economy_nation);
    break;
  case AIState::Expanding:
    step_expanding(ctx, no_economy_nation);
    break;
  }

  if (ctx.state != previous_state) {
    note_state_change(ctx, previous_state);
  }
}

void AIReasoner::validate_state(AIContext& ctx) {

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
      ctx.state = (is_no_economy_nation(ctx) && ctx.has_base_anchor)
                      ? AIState::Gathering
                      : AIState::Idle;
      ctx.state_timer = 0.0F;
    }
  }

  if ((is_no_economy_nation(ctx) || holds_garrison(ctx.strategy_config)) &&
      (ctx.state == AIState::Expanding || ctx.state == AIState::Attacking)) {
    ctx.state = has_active_local_threat(ctx) ? AIState::Defending : AIState::Gathering;
    ctx.state_timer = 0.0F;
  }

  if (ctx.state_timer > MAX_STATE_TIMER) {
    ctx.state_timer = ctx.max_state_duration;
  }
  if (ctx.decision_timer > MAX_DECISION_TIMER) {
    ctx.decision_timer = 0.0F;
  }

  size_t const max_expected_assignments =
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
