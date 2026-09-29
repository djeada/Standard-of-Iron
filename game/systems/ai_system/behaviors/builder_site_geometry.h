#pragma once

#include <string>

#include "../ai_types.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

inline constexpr float k_slot_gap = 0.2F;

[[nodiscard]] auto footprint_half_extent(const std::string& building_type) -> float;

[[nodiscard]] auto slot_clearance(const char* building_type,
                                  Game::Units::SpawnType standing) -> float;

[[nodiscard]] auto site_is_free(const AISnapshot& snapshot,
                                const char* building_type,
                                float world_x,
                                float world_z) -> bool;

void clamp_to_map_bounds(const AISnapshot& snapshot, float& x, float& z);

} // namespace Game::Systems::AI
