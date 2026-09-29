#include "ai_expansion_planner.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../core/ownership_constants.h"
#include "ai_base_manager.h"
#include "ai_doctrine_rules.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_outpost_site_min_objective_distance = 36.0F;
constexpr float k_outpost_structure_radius = 16.0F;
constexpr float k_outpost_pending_radius = 10.0F;
constexpr float k_outpost_site_lateral_step = 16.0F;
constexpr int k_outpost_site_attempts = 5;

auto desired_outpost_barracks_count(const AIStrategyConfig& strategy) -> int {
  return std::max(0, strategy.desired_outpost_barracks_count);
}

auto expansion_force_threshold(const AIContext& ctx) -> int {
  const int priority_threshold = static_cast<int>(
      std::ceil(4.0F / std::max(0.25F, ctx.strategy_config.expansion_priority)));
  return std::max({2, reactive_attack_size(ctx.strategy_config), priority_threshold});
}

auto needs_outpost_construction(const AIContext& ctx) -> bool {
  if (is_no_economy_nation(ctx)) {
    return false;
  }
  if (!ctx.has_expansion_site) {
    return false;
  }

  if (ctx.outpost_barracks_count <
      desired_outpost_barracks_count(ctx.strategy_config)) {
    return true;
  }

  return ctx.outpost_barracks_count >=
             desired_outpost_barracks_count(ctx.strategy_config) &&
         ctx.outpost_home_count < ctx.strategy_config.outpost_home_target;
}

auto select_enemy_expansion_objective(const AISnapshot& snapshot,
                                      const AIContext& ctx) -> const ContactSnapshot* {
  const ContactSnapshot* best = nullptr;
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (const auto& objective : snapshot.strategic_objectives) {
    if (Game::Core::is_neutral_owner(objective.owner_id)) {
      continue;
    }

    const float distance_sq = distance_squared(objective.pos_x,
                                               objective.pos_y,
                                               objective.pos_z,
                                               ctx.base_pos_x,
                                               ctx.base_pos_y,
                                               ctx.base_pos_z);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best = &objective;
    }
  }

  return best;
}

auto choose_forward_site(const AISnapshot& snapshot, AIContext& ctx) -> bool {
  const auto* objective = select_enemy_expansion_objective(snapshot, ctx);
  if (objective == nullptr) {
    return false;
  }

  const float dx = objective->pos_x - ctx.base_pos_x;
  const float dz = objective->pos_z - ctx.base_pos_z;
  const float objective_distance = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
  if (objective_distance < k_outpost_site_min_objective_distance) {
    return false;
  }

  const float site_distance =
      std::min(ctx.strategy_config.expansion_site_distance, objective_distance * 0.5F);
  const float forward_x = ctx.base_pos_x + (dx / objective_distance) * site_distance;
  const float forward_z = ctx.base_pos_z + (dz / objective_distance) * site_distance;
  const float lateral_x = -dz / objective_distance;
  const float lateral_z = dx / objective_distance;

  for (int attempt = 0; attempt < k_outpost_site_attempts; ++attempt) {
    const float lateral_offset = k_outpost_site_lateral_step *
                                 static_cast<float>((attempt + 1) / 2) *
                                 ((attempt % 2 == 0) ? 1.0F : -1.0F);
    const float candidate_x = forward_x + lateral_x * lateral_offset;
    const float candidate_z = forward_z + lateral_z * lateral_offset;

    if (AIBaseManager::site_is_abandoned(
            ctx, candidate_x, candidate_z, snapshot.game_time)) {
      continue;
    }

    ctx.expansion_site_x = candidate_x;
    ctx.expansion_site_z = candidate_z;
    return true;
  }
  return false;
}

void count_outpost_progress(const AISnapshot& snapshot, AIContext& ctx) {
  const float outpost_radius_sq =
      k_outpost_structure_radius * k_outpost_structure_radius;
  const float pending_radius_sq = k_outpost_pending_radius * k_outpost_pending_radius;

  for (const auto& entity : snapshot.friendly_units) {
    const float site_distance_sq = distance_squared(entity.pos_x,
                                                    entity.pos_y,
                                                    entity.pos_z,
                                                    ctx.expansion_site_x,
                                                    0.0F,
                                                    ctx.expansion_site_z);

    if (entity.is_building && site_distance_sq <= outpost_radius_sq) {
      if (entity.spawn_type == Game::Units::SpawnType::Barracks) {
        ctx.outpost_barracks_count++;
      } else if (entity.spawn_type == Game::Units::SpawnType::Home) {
        ctx.outpost_home_count++;
      }
    }

    if (entity.spawn_type == Game::Units::SpawnType::Builder &&
        entity.builder_production.has_component &&
        entity.builder_production.has_construction_site) {
      const float pending_distance_sq =
          distance_squared(entity.builder_production.construction_site_x,
                           0.0F,
                           entity.builder_production.construction_site_z,
                           ctx.expansion_site_x,
                           0.0F,
                           ctx.expansion_site_z);
      if (pending_distance_sq <= pending_radius_sq) {
        ctx.expansion_construction_pending = true;
      }
    }
  }
}

auto can_capture_neutral_expansion(const AIContext& ctx) -> bool {
  if (is_no_economy_nation(ctx)) {
    return false;
  }
  return ctx.strategy_config.expansion_priority > 0.8F &&
         ctx.neutral_barracks_count > 0 &&
         committable_attack_force(ctx) >= expansion_force_threshold(ctx);
}

auto can_build_outpost_expansion(const AIContext& ctx) -> bool {
  if (is_no_economy_nation(ctx)) {
    return false;
  }
  if (ctx.strategy_config.expansion_priority <= 0.8F || !ctx.has_expansion_site ||
      ctx.home_count < ctx.strategy_config.base_home_target ||
      ctx.barracks_count == 0 || ctx.builder_count == 0) {
    return false;
  }

  if (!(needs_outpost_construction(ctx) || ctx.expansion_construction_pending)) {
    return false;
  }

  return committable_attack_force(ctx) >= expansion_force_threshold(ctx);
}

} // namespace

void update_expansion_site(const AISnapshot& snapshot,
                           AIContext& ctx,
                           bool had_previous_site,
                           float previous_site_x,
                           float previous_site_z) {
  ctx.has_expansion_site = false;
  ctx.outpost_barracks_count = 0;
  ctx.outpost_home_count = 0;
  ctx.expansion_construction_pending = false;
  ctx.forward_plan.has_site = false;

  if (!ctx.anchor_is_structural || holds_garrison(ctx.strategy_config) ||
      desired_outpost_barracks_count(ctx.strategy_config) <= 0) {
    return;
  }

  const bool previous_site_usable =
      had_previous_site &&
      !AIBaseManager::site_is_abandoned(
          ctx, previous_site_x, previous_site_z, snapshot.game_time);

  if (previous_site_usable) {
    ctx.has_expansion_site = true;
    ctx.expansion_site_x = previous_site_x;
    ctx.expansion_site_z = previous_site_z;
  } else {
    if (!choose_forward_site(snapshot, ctx)) {
      return;
    }
    ctx.has_expansion_site = true;
  }

  ctx.forward_plan.has_site = true;
  ctx.forward_plan.site_x = ctx.expansion_site_x;
  ctx.forward_plan.site_z = ctx.expansion_site_z;

  count_outpost_progress(snapshot, ctx);
}

auto wants_expansion(const AIContext& ctx) -> bool {
  if (holds_garrison(ctx.strategy_config)) {
    return false;
  }
  return can_capture_neutral_expansion(ctx) || can_build_outpost_expansion(ctx);
}

} // namespace Game::Systems::AI
