#include <gtest/gtest.h>

#include "app/core/mission_commander_setup.h"

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

  const auto resolved =
      App::Core::resolve_commander_position(units, {}, {}, {90.0F, 90.0F});

  EXPECT_EQ(resolved.space, App::Core::CommanderPositionSpace::Mission);
  EXPECT_FLOAT_EQ(resolved.position.x, 12.0F);
  EXPECT_FLOAT_EQ(resolved.position.z, 22.0F);
}

TEST(MissionCommanderSetupTest, UsesExistingWorldTroopsWhenMissionHasNoSpawns) {
  std::vector<App::Core::ExistingOwnerSpawnAnchor> existing_spawns = {
      {{-64.5F, -36.5F}, false},
      {{-64.5F, -32.5F}, false},
      {{-68.5F, -34.5F}, false},
      {{-68.5F, -38.5F}, false},
      {{-62.5F, -40.5F}, false},
      {{-62.5F, -28.5F}, false},
      {{-70.5F, -36.5F}, false},
      {{-70.5F, -32.5F}, false},
      {{-69.5F, -34.5F}, false},
  };

  const auto resolved =
      App::Core::resolve_commander_position({}, {}, existing_spawns, {68.0F, 70.0F});

  EXPECT_EQ(resolved.space, App::Core::CommanderPositionSpace::World);
  EXPECT_FLOAT_EQ(resolved.position.x, -66.8333359F);
  EXPECT_FLOAT_EQ(resolved.position.z, -34.9444427F);
}

TEST(MissionCommanderSetupTest,
     UsesLocalWorldClusterWhenExistingSpawnsAreSpreadAcrossMap) {
  std::vector<App::Core::ExistingOwnerSpawnAnchor> existing_spawns = {
      {{32.5F, 57.5F}, false},
      {{36.5F, 57.5F}, false},
      {{35.5F, 61.5F}, false},
      {{63.5F, 57.5F}, false},
      {{67.5F, 57.5F}, false},
  };

  const auto resolved =
      App::Core::resolve_commander_position({}, {}, existing_spawns, {132.0F, 80.0F});

  EXPECT_EQ(resolved.space, App::Core::CommanderPositionSpace::World);
  EXPECT_FLOAT_EQ(resolved.position.x, 34.8333321F);
  EXPECT_FLOAT_EQ(resolved.position.z, 58.8333321F);
}
