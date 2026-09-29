#include "ai_doctrine_rules.h"

#include <algorithm>

#include "../nation_registry.h"

namespace Game::Systems::AI {

namespace {
constexpr float k_attack_initiation_aggression_threshold = 0.70F;
constexpr float k_local_threat_memory_duration = 6.0F;
} // namespace

auto holds_garrison(const AIStrategyConfig& strategy) -> bool {
  return strategy.posture == AIPosture::Garrison;
}

auto can_initiate_attack(const AIStrategyConfig& strategy) -> bool {
  return !holds_garrison(strategy) &&
         strategy.aggression_modifier >= k_attack_initiation_aggression_threshold;
}

auto is_no_economy_nation(const AIContext& ctx) -> bool {
  return ctx.nation != nullptr && !ctx.nation->has_economy;
}

auto reactive_attack_size(const AIStrategyConfig& strategy) -> int {
  return std::max(1, strategy.reactive_attack_size);
}

auto proactive_attack_size(const AIStrategyConfig& strategy) -> int {
  return std::max(strategy.reactive_attack_size, strategy.proactive_attack_size);
}

auto ready_attack_force(const AIContext& ctx) -> int {
  return ctx.anchor_is_structural
             ? ctx.assembled_unit_count
             : std::max(
                   0, ctx.total_units - ctx.builder_count - ctx.effective_harass_units);
}

auto committable_attack_force(const AIContext& ctx) -> int {
  return std::max(0, ready_attack_force(ctx) - ctx.effective_reserve_units);
}

auto resume_attack_health_threshold(const AIStrategyConfig& strategy) -> float {
  return std::clamp(0.65F + 0.10F * (strategy.defense_modifier - 1.0F), 0.60F, 0.90F);
}

auto return_to_idle_health_threshold(const AIStrategyConfig& strategy) -> float {
  return std::clamp(0.80F + 0.05F * (strategy.defense_modifier - 1.0F), 0.75F, 0.95F);
}

auto has_active_local_threat(const AIContext& ctx) -> bool {
  return ctx.barracks_under_threat || ctx.any_base_under_threat ||
         !ctx.buildings_under_attack.empty() || (ctx.nearby_threat_count > 0);
}

auto has_recent_local_threat(const AIContext& ctx, float game_time) -> bool {
  return has_active_local_threat(ctx) ||
         ((ctx.last_local_threat_time > 0.0F) &&
          ((game_time - ctx.last_local_threat_time) <= k_local_threat_memory_duration));
}

} // namespace Game::Systems::AI
