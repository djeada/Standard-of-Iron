#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// The RPG commander walking through friendly ranks, workers, livestock and streams.
[[nodiscard]] auto
build_rpg_friendly_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
