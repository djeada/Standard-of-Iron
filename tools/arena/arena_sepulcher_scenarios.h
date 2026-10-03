#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// The Iron Sepulcher roster, spells, shrines, awakening waves and demolition scenes.
[[nodiscard]] auto
build_sepulcher_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
