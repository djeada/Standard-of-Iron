#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>

#include "game/units/spawn_type.h"
#include "tools/arena/arena_scenario.h"
#include "tools/arena/arena_scenarios.h"

TEST(SlopeScan, NothingBuiltOnTheSacredMountain) {
  const Arena::ArenaScenarioDefinition* scenario = nullptr;
  for (const auto& candidate : Arena::Scenarios::definitions()) {
    if (candidate.id == QLatin1String(Arena::Scenarios::k_imperial_capital_id)) {
      scenario = &candidate;
      break;
    }
  }
  ASSERT_NE(scenario, nullptr);

  float centre_x = 0.0F;
  float centre_z = 0.0F;
  float plateau = 0.0F;
  float radius = 0.0F;
  for (const auto& patch : scenario->elevation_patches) {
    if (patch.radius > radius) {
      radius = patch.radius;
      plateau = patch.plateau;
      centre_x = patch.center.x();
      centre_z = patch.center.z();
    }
  }
  std::printf("summit (%.0f,%.0f) plateau %.0f radius %.0f\n",
              centre_x,
              centre_z,
              plateau,
              radius);

  int on_slope = 0;
  for (const auto& group : scenario->groups) {
    if (!group.spawn_type.has_value() ||
        !Game::Units::is_building_spawn(*group.spawn_type)) {
      continue;
    }
    const float distance =
        std::hypot(group.origin.x() - centre_x, group.origin.z() - centre_z);
    if (distance > plateau && distance <= radius) {
      ++on_slope;
      if (on_slope <= 25) {
        std::printf("  SLOPE d=%6.1f  %s (%.0f, %.0f)\n",
                    distance,
                    group.name.toStdString().c_str(),
                    group.origin.x(),
                    group.origin.z());
      }
    }
  }
  EXPECT_EQ(on_slope, 0) << "the sacred mountain slopes must stay clear of structures";
}
