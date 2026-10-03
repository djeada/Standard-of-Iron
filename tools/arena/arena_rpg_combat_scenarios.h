#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// The RPG commander attack scenes: one press one swing, the buffer window, whiffs,
// stamina, lock cycling and the weapon grammars.
[[nodiscard]] auto
build_rpg_combat_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
