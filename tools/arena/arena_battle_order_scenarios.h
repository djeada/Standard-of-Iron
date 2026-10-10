#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Historical battle orders fought through the real formation system: Cannae
// (deep triplex acies against Hannibal's yielding crescent) and Zama (Scipio's
// open lanes against the elephant screen).
[[nodiscard]] auto
build_battle_order_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
