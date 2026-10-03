#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Identity lineups for commanders, healers, troops, workers and helmets, and the
// settlement works scenes.
[[nodiscard]] auto build_identity_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
