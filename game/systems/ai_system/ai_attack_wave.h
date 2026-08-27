#pragma once

#include <vector>

#include "ai_types.h"

namespace Game::Systems::AI {

void update_attack_wave(const AISnapshot& snapshot, AIContext& context);

[[nodiscard]] auto
wave_force_units(const AISnapshot& snapshot,
                 const AIContext& context) -> std::vector<const EntitySnapshot*>;

[[nodiscard]] auto wave_size_for(const AIContext& context) -> int;

[[nodiscard]] auto garrison_target_for(const AIContext& context,
                                       int combat_unit_count) -> int;

} // namespace Game::Systems::AI
