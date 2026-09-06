#pragma once

#include <QString>
#include <QVector3D>

#include <vector>

#include "tools/arena/arena_scenario.h"

namespace Arena::Scenarios {

struct CityPatrolRoute {
  QString name;
  std::vector<QVector3D> route;
};

struct CityPlan {
  ArenaScenarioDefinition definition;
  std::vector<CityPatrolRoute> patrols;
};

[[nodiscard]] auto authored_city_plan() -> CityPlan;

} // namespace Arena::Scenarios
