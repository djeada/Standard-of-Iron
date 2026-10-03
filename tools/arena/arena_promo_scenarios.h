#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// The promo scenes: last stand, night of the dead, storm charge, commander duel, rally
// and wolf attack.
[[nodiscard]] auto build_promo_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
