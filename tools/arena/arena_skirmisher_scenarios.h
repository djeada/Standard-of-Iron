#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Balearic slingers and Roman velites: a close lineup of both skirmishers, and
// an opening exchange in which each screen fires ahead of its line and falls
// back behind it once the enemy foot closes.
[[nodiscard]] auto
build_skirmisher_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
