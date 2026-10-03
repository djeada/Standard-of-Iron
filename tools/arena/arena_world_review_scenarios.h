#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Camps, trade towns, gait and prop reviews, and the architecture and fortification
// showcases.
[[nodiscard]] auto
build_world_review_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
