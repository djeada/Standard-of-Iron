#pragma once

#include "../marketplace_system.h"
#include "../resource_types.h"
#include "ai_types.h"

namespace Game::Systems::AI {

[[nodiscard]] auto ally_generosity(const AIStrategyConfig& config) -> float;

[[nodiscard]] auto answer_ally_request(const AIStrategyConfig& config,
                                       const ResourceAmounts& stock,
                                       const AllyTributeRequest& request,
                                       bool under_attack) -> AllyTributeAnswer;

} // namespace Game::Systems::AI
