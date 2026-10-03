#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// Sustained, continuity and massed battles, up to campaign scale.
[[nodiscard]] auto
build_battle_scale_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
