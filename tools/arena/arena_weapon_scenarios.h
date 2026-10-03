#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Weapon duels, charges, siege engine impacts, structure assaults and the mounted
// exchanges.
[[nodiscard]] auto build_weapon_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
