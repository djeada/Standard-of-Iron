#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

namespace AIReasoner {

void update_context(const AISnapshot& snapshot, AIContext& ctx);

void update_state_machine(const AISnapshot& snapshot, AIContext& ctx, float delta_time);

void validate_state(AIContext& ctx);

} // namespace AIReasoner

} // namespace Game::Systems::AI
