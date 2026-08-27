#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

[[nodiscard]] auto build_ai_duel_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
