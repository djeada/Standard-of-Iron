#pragma once

#include <optional>

#include "../alliance_board.h"
#include "../marketplace_system.h"
#include "../resource_types.h"
#include "ai_types.h"

namespace Game::Systems::AI {

[[nodiscard]] auto ally_generosity(const AIStrategyConfig& config) -> float;

[[nodiscard]] auto answer_ally_request(const AIStrategyConfig& config,
                                       const ResourceAmounts& stock,
                                       const AllyTributeRequest& request,
                                       bool under_attack) -> AllyTributeAnswer;

struct AllyPleaNeed {
  ResourceType resource = ResourceType::Gold;
  int amount = 0;
};

[[nodiscard]] auto
pick_ally_plea(const ResourceAmounts& own_stock,
               const ResourceAmounts& ally_stock) -> std::optional<AllyPleaNeed>;

struct AllyCallStanding {
  bool under_threat = false;
  int spare_units = 0;
};

inline constexpr int k_ally_call_min_defenders = 3;
inline constexpr int k_ally_call_min_attackers = 4;

[[nodiscard]] auto answer_ally_call(const AIStrategyConfig& config,
                                    const AllyCallStanding& standing,
                                    AllyCallKind kind) -> AllyCallVerdict;

} // namespace Game::Systems::AI
