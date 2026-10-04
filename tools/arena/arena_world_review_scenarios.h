#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

[[nodiscard]] auto
build_world_review_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
