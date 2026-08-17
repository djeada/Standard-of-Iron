#pragma once

#include <vector>

#include "ai_behavior_registry.h"
#include "ai_types.h"

namespace Game::Systems::AI {

namespace AIExecutor {

void run(const AISnapshot& snapshot,
         AIContext& context,
         float delta_time,
         AIBehaviorRegistry& registry,
         std::vector<AICommand>& out_commands);

} // namespace AIExecutor

} // namespace Game::Systems::AI
