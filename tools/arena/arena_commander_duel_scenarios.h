#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

[[nodiscard]] auto
build_commander_duel_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
