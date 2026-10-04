#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

[[nodiscard]] auto
build_rpg_combat_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
