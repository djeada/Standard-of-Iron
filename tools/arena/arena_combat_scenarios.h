#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Melee locks, chase and retreat transitions, hold stances, idle ambience and small
// skirmish scenes.
[[nodiscard]] auto build_combat_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
