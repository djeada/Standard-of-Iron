#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Friendly passage through gates, formation crossings and fog of war recon.
[[nodiscard]] auto build_gate_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
