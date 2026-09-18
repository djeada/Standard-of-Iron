#pragma once

#include <QRectF>

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

[[nodiscard]] auto build_city_definitions() -> std::vector<ArenaScenarioDefinition>;

void dress_aurelia_magna(ArenaScenarioDefinition& scenario, const QRectF& clear);

} // namespace Arena::Scenarios
