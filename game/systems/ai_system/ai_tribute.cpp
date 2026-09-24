#include "ai_tribute.h"

#include <algorithm>
#include <cmath>

#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

auto strategy_bias(AIStrategy strategy) -> float {
  switch (strategy) {
  case AIStrategy::Economic:
    return 0.20F;
  case AIStrategy::Defensive:
    return 0.10F;
  case AIStrategy::Expansionist:
    return 0.05F;
  case AIStrategy::Aggressive:
    return -0.10F;
  case AIStrategy::Rusher:
    return -0.15F;
  case AIStrategy::Harasser:
    return -0.05F;
  case AIStrategy::Balanced:
  case AIStrategy::SepulcherDefense:
    break;
  }
  return 0.0F;
}

auto reserve_for(ResourceType type, float aggression) -> int {
  int base = 100;
  switch (type) {
  case ResourceType::Gold:
    base = 150;
    break;
  case ResourceType::Food:
    base = 120;
    break;
  case ResourceType::Wood:
    base = 150;
    break;
  case ResourceType::Stone:
    base = 80;
    break;
  case ResourceType::Iron:
    base = 60;
    break;
  default:
    break;
  }
  return static_cast<int>(static_cast<float>(base) *
                          (1.0F + 0.5F * std::clamp(aggression, 0.0F, 1.0F)));
}

} // namespace

auto ally_generosity(const AIStrategyConfig& config) -> float {
  const auto& p = config.personality;
  const float value = 0.45F + 0.35F * p.defense - 0.30F * p.aggression -
                      0.05F * p.harassment + strategy_bias(config.strategy);
  return std::clamp(value, 0.05F, 0.95F);
}

auto answer_ally_request(const AIStrategyConfig& config,
                         const ResourceAmounts& stock,
                         const AllyTributeRequest& request,
                         bool under_attack) -> AllyTributeAnswer {
  AllyTributeAnswer answer{.requester = request.requester,
                           .giver = request.giver,
                           .resource = request.resource,
                           .requested = request.amount,
                           .granted = 0,
                           .verdict = AllyTributeVerdict::RefusedShort};
  const int wanted = std::clamp(request.amount, 0, k_max_ally_tribute);
  if (wanted <= 0) {
    return answer;
  }
  const int surplus = stock.get(request.resource) -
                      reserve_for(request.resource, config.personality.aggression);
  if (surplus <= 0) {
    return answer;
  }
  float offer = static_cast<float>(surplus) * ally_generosity(config);
  if (under_attack) {
    offer *= 0.3F;
  }
  const int offered = static_cast<int>(std::floor(offer / 5.0F)) * 5;
  if (offered >= wanted) {
    answer.granted = wanted;
    answer.verdict = AllyTributeVerdict::Granted;
  } else if (offered >= 10 &&
             static_cast<float>(offered) >= 0.4F * static_cast<float>(wanted)) {
    answer.granted = offered;
    answer.verdict = AllyTributeVerdict::Partial;
  } else {
    answer.verdict = surplus >= wanted ? AllyTributeVerdict::RefusedStingy
                                       : AllyTributeVerdict::RefusedShort;
  }
  return answer;
}

auto pick_ally_plea(const ResourceAmounts& own_stock,
                    const ResourceAmounts& ally_stock) -> std::optional<AllyPleaNeed> {
  constexpr float k_desperate_share = 0.35F;
  std::optional<AllyPleaNeed> best;
  float best_share = k_desperate_share;
  for (const auto type : {ResourceType::Food,
                          ResourceType::Wood,
                          ResourceType::Gold,
                          ResourceType::Stone,
                          ResourceType::Iron}) {
    const int need = reserve_for(type, 0.0F);
    const int held = std::max(0, own_stock.get(type));
    const float share = static_cast<float>(held) / static_cast<float>(need);
    if (share >= best_share) {
      continue;
    }
    const int shortfall = need - held;
    const int amount = std::clamp((shortfall + 24) / 25 * 25, 50, 200);
    if (ally_stock.get(type) < amount + need) {
      continue;
    }
    best_share = share;
    best = AllyPleaNeed{.resource = type, .amount = amount};
  }
  return best;
}

auto answer_ally_call(const AIStrategyConfig& config,
                      const AllyCallStanding& standing,
                      AllyCallKind kind) -> AllyCallVerdict {
  constexpr float k_willing = 0.35F;
  if (standing.under_threat) {
    return AllyCallVerdict::RefusedUnderThreat;
  }
  const auto& p = config.personality;
  if (kind == AllyCallKind::Defend) {
    if (standing.spare_units < k_ally_call_min_defenders) {
      return AllyCallVerdict::RefusedNoArmy;
    }
    const float willing = 0.50F + 0.40F * p.defense - 0.20F * p.aggression +
                          0.5F * strategy_bias(config.strategy);
    return willing >= k_willing ? AllyCallVerdict::Accepted
                                : AllyCallVerdict::RefusedUnwilling;
  }
  if (!commander_may_attack(config) ||
      (config.posture == AIPosture::Garrison && config.doctrine == nullptr)) {
    return AllyCallVerdict::RefusedUnwilling;
  }
  if (standing.spare_units < k_ally_call_min_attackers) {
    return AllyCallVerdict::RefusedNoArmy;
  }
  const float willing = 0.40F + 0.50F * p.aggression - 0.20F * p.defense -
                        0.5F * strategy_bias(config.strategy);
  return willing >= k_willing ? AllyCallVerdict::Accepted
                              : AllyCallVerdict::RefusedUnwilling;
}

} // namespace Game::Systems::AI
