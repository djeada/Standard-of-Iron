#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Commander against commander duels, signature matchups and press scenes.
[[nodiscard]] auto
build_commander_duel_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
