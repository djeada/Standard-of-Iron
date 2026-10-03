#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Sanctuary and weather fixtures, the lighting sweeps and parity scenes.
[[nodiscard]] auto build_lighting_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
