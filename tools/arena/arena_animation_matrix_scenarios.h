#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// The locomotion, damage and action transition matrices.
[[nodiscard]] auto
build_animation_matrix_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
