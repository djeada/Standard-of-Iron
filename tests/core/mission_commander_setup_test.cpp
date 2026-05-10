#include "app/core/mission_commander_setup.h"

#include <gtest/gtest.h>

TEST(MissionCommanderSetupTest, FallsBackToHannibalForCarthage) {
  EXPECT_EQ(App::Core::resolve_commander_troop("carthage", std::nullopt),
            QStringLiteral("carthage_elephant_master"));
}

TEST(MissionCommanderSetupTest, KeepsConfiguredCommanderWhenPresent) {
  EXPECT_EQ(App::Core::resolve_commander_troop(
                "carthage", QStringLiteral("carthage_cavalry_patron")),
            QStringLiteral("carthage_cavalry_patron"));
}

TEST(MissionCommanderSetupTest, PrefersAuthoredTroopPositions) {
  std::vector<Game::Mission::UnitSetup> units = {
      {.type = "spearman", .count = 2, .position = {10.0F, 20.0F}},
      {.type = "archer", .count = 1, .position = {16.0F, 26.0F}}};

  const auto position =
      App::Core::resolve_commander_position(units, {}, {}, {90.0F, 90.0F});

  EXPECT_FLOAT_EQ(position.x, 12.0F);
  EXPECT_FLOAT_EQ(position.z, 22.0F);
}

TEST(MissionCommanderSetupTest, UsesExistingMapTroopsWhenMissionHasNoSpawns) {
  std::vector<App::Core::ExistingOwnerSpawnAnchor> existing_spawns = {
      {{10.0F, 38.0F}, false}, {{10.0F, 42.0F}, false}, {{6.0F, 40.0F}, false},
      {{6.0F, 36.0F}, false},  {{12.0F, 34.0F}, false}, {{12.0F, 46.0F}, false},
      {{4.0F, 38.0F}, false},  {{4.0F, 42.0F}, false},  {{5.0F, 40.0F}, false},
  };

  const auto position = App::Core::resolve_commander_position(
      {}, {}, existing_spawns, {68.0F, 70.0F});

  EXPECT_FLOAT_EQ(position.x, 7.66666651F);
  EXPECT_FLOAT_EQ(position.z, 39.5555573F);
}

TEST(MissionCommanderSetupTest,
     UsesLocalClusterWhenExistingSpawnsAreSpreadAcrossMap) {
  std::vector<App::Core::ExistingOwnerSpawnAnchor> existing_spawns = {
      {{108.0F, 132.0F}, false}, {{112.0F, 132.0F}, false},
      {{110.0F, 136.0F}, false}, {{138.0F, 132.0F}, false},
      {{142.0F, 132.0F}, false},
  };

  const auto position = App::Core::resolve_commander_position(
      {}, {}, existing_spawns, {132.0F, 80.0F});

  EXPECT_FLOAT_EQ(position.x, 110.0F);
  EXPECT_FLOAT_EQ(position.z, 133.333328F);
}
