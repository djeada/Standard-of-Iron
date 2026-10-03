#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// The range indicator scenes and the unarmed support brawl.
[[nodiscard]] auto
build_range_indicator_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
