#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Carthage's Gallic and Iberian allies: a side-by-side identity lineup against
// Roman and Carthaginian troops, and a small Cannae-style clash in which the
// allies hold their own owner slots on Carthage's team.
[[nodiscard]] auto
build_allied_nation_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
