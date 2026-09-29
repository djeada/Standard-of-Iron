#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

[[nodiscard]] auto compute_macro_targets(const AISnapshot& snapshot,
                                         const AIContext& ctx,
                                         int catapult_count) -> AIContext::MacroTargets;

} // namespace Game::Systems::AI
