#include <gtest/gtest.h>

#include "tools/arena/unit_spawn_options.h"

TEST(ArenaUnitSpawnOptionsTest, ParsesSingleHorsePreset) {
  auto const option = Arena::UnitSpawnOptions::parse_special_unit_option(
      Arena::UnitSpawnOptions::build_special_unit_id(
          Arena::UnitSpawnOptions::Kind::SingleHorse,
          Game::Units::TroopType::MountedKnight));

  ASSERT_TRUE(option.has_value());
  EXPECT_EQ(option->kind, Arena::UnitSpawnOptions::Kind::SingleHorse);
  EXPECT_EQ(option->troop_type, Game::Units::TroopType::MountedKnight);
  EXPECT_EQ(option->individuals_per_unit, 1);
  EXPECT_FALSE(option->render_rider);
}

TEST(ArenaUnitSpawnOptionsTest, ParsesSingleElephantPreset) {
  auto const option = Arena::UnitSpawnOptions::parse_special_unit_option(
      Arena::UnitSpawnOptions::build_special_unit_id(
          Arena::UnitSpawnOptions::Kind::SingleElephant,
          Game::Units::TroopType::Elephant));

  ASSERT_TRUE(option.has_value());
  EXPECT_EQ(option->kind, Arena::UnitSpawnOptions::Kind::SingleElephant);
  EXPECT_EQ(option->troop_type, Game::Units::TroopType::Elephant);
  EXPECT_EQ(option->individuals_per_unit, 1);
  EXPECT_TRUE(option->render_rider);
}

TEST(ArenaUnitSpawnOptionsTest, RejectsUnknownPresetIds) {
  EXPECT_FALSE(Arena::UnitSpawnOptions::parse_special_unit_option(
                   QStringLiteral("arena_single_horse:not_a_unit"))
                   .has_value());
  EXPECT_FALSE(Arena::UnitSpawnOptions::parse_special_unit_option(
                   QStringLiteral("horse_swordsman"))
                   .has_value());
}
