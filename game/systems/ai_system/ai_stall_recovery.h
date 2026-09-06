#pragma once

#include <vector>

#include "ai_types.h"

namespace Game::Systems::AI {

inline constexpr int k_max_stall_nudges = 3;
inline constexpr float k_stall_nudge_interval_seconds = 4.0F;
inline constexpr float k_stall_stand_down_seconds = 20.0F;
inline constexpr float k_stall_detour_metres = 7.0F;
inline constexpr int k_stall_abandon_patience = 2;

[[nodiscard]] auto is_stood_down(Engine::Core::EntityID unit_id,
                                 const AIContext& context,
                                 float current_time) -> bool;

[[nodiscard]] auto is_going_nowhere(const EntitySnapshot& entity) -> bool;

void update_stall_recovery(const AISnapshot& snapshot,
                           AIContext& context,
                           std::vector<AICommand>& out_commands);

} // namespace Game::Systems::AI
