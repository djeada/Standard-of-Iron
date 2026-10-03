#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Path crossing scenes and the road showcases.
[[nodiscard]] auto build_pathing_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
