#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

void update_expansion_site(const AISnapshot& snapshot,
                           AIContext& ctx,
                           bool had_previous_site,
                           float previous_site_x,
                           float previous_site_z);

[[nodiscard]] auto wants_expansion(const AIContext& ctx) -> bool;

} // namespace Game::Systems::AI
