#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

void update_assault_unit_ids(const AISnapshot& snapshot, AIContext& ctx);
void update_reserve_unit_ids(const AISnapshot& snapshot, AIContext& ctx);
void update_harass_unit_ids(const AISnapshot& snapshot, AIContext& ctx);

[[nodiscard]] auto committed_army_count(const AISnapshot& snapshot,
                                        const AIContext& ctx) -> int;

} // namespace Game::Systems::AI
