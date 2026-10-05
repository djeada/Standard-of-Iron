#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

inline constexpr char k_duel_reel_old_enemies_id[] = "duel_reel_old_enemies";
inline constexpr char k_duel_reel_alpine_pass_id[] = "duel_reel_alpine_pass";
inline constexpr char k_duel_reel_night_raid_id[] = "duel_reel_night_raid";

[[nodiscard]] auto
build_duel_reel_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
