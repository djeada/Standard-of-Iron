#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Rival economies and the village, colony, outpost and convoy life scenes, with the
// water and wall corner showcases.
[[nodiscard]] auto
build_settlement_life_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
