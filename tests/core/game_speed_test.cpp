#include <QJsonObject>

#include <gtest/gtest.h>
#include <limits>

#include "app/core/game_speed.h"
#include "game/core/world.h"
#include "game/render_bridge/game_state_serializer.h"

namespace {

namespace Speed = App::Core::GameSpeed;

TEST(GameSpeedTest, TheCatalogueRunsFromHalfSpeedToQuadruple) {
  ASSERT_FALSE(Speed::k_options.empty());
  EXPECT_FLOAT_EQ(Speed::k_min, 0.5F);
  EXPECT_FLOAT_EQ(Speed::k_max, 4.0F);
  EXPECT_TRUE(Speed::is_supported(Speed::k_default));

  for (std::size_t index = 1; index < Speed::k_options.size(); ++index) {
    EXPECT_GT(Speed::k_options.at(index), Speed::k_options.at(index - 1));
  }
}

TEST(GameSpeedTest, EveryOfferedSpeedSurvivesSanitising) {
  for (const float option : Speed::k_options) {
    EXPECT_FLOAT_EQ(Speed::sanitize(option), option);
  }
}

TEST(GameSpeedTest, OutOfRangeAndBrokenSpeedsSnapBackIntoTheCatalogue) {
  EXPECT_FLOAT_EQ(Speed::sanitize(0.0F), Speed::k_min);
  EXPECT_FLOAT_EQ(Speed::sanitize(-3.0F), Speed::k_min);
  EXPECT_FLOAT_EQ(Speed::sanitize(1000.0F), Speed::k_max);
  EXPECT_FLOAT_EQ(Speed::sanitize(std::numeric_limits<float>::quiet_NaN()),
                  Speed::k_default);
  EXPECT_FLOAT_EQ(Speed::sanitize(std::numeric_limits<float>::infinity()),
                  Speed::k_default);

  EXPECT_FLOAT_EQ(Speed::sanitize(2.9F), 3.0F);
  EXPECT_FLOAT_EQ(Speed::sanitize(0.6F), 0.5F);
}

TEST(GameSpeedTest, SteppingWalksTheCatalogueAndStopsAtTheEnds) {
  EXPECT_FLOAT_EQ(Speed::stepped(1.0F, 1), 2.0F);
  EXPECT_FLOAT_EQ(Speed::stepped(2.0F, 1), 3.0F);
  EXPECT_FLOAT_EQ(Speed::stepped(3.0F, 1), 4.0F);
  EXPECT_FLOAT_EQ(Speed::stepped(4.0F, 1), 4.0F);

  EXPECT_FLOAT_EQ(Speed::stepped(1.0F, -1), 0.5F);
  EXPECT_FLOAT_EQ(Speed::stepped(0.5F, -1), 0.5F);

  EXPECT_FLOAT_EQ(Speed::stepped(0.0F, 1), 1.0F);
  EXPECT_FLOAT_EQ(Speed::stepped(99.0F, -1), 3.0F);
}

TEST(GameSpeedTest, ASaveMadeAtQuadrupleSpeedComesBackAtQuadrupleSpeed) {
  Engine::Core::World world;
  Game::Systems::LevelSnapshot level;

  for (const float option : Speed::k_options) {
    Game::Systems::RuntimeSnapshot saved;
    saved.time_scale = option;

    const QJsonObject metadata = Game::Systems::GameStateSerializer::build_metadata(
        world, nullptr, level, saved);

    Game::Systems::RuntimeSnapshot restored;
    Game::Systems::GameStateSerializer::restore_runtime_from_metadata(metadata,
                                                                      restored);

    EXPECT_FLOAT_EQ(restored.time_scale, option);
    EXPECT_FLOAT_EQ(Speed::sanitize(restored.time_scale), option);
  }
}

TEST(GameSpeedTest, ASaveCarryingAnImpossibleSpeedLoadsAtAPlayableOne) {
  Engine::Core::World world;
  Game::Systems::LevelSnapshot level;

  for (const float stored : {-1.0F, 0.0F, 7.5F, 250.0F}) {
    Game::Systems::RuntimeSnapshot saved;
    saved.time_scale = stored;

    const QJsonObject metadata = Game::Systems::GameStateSerializer::build_metadata(
        world, nullptr, level, saved);

    Game::Systems::RuntimeSnapshot restored;
    Game::Systems::GameStateSerializer::restore_runtime_from_metadata(metadata,
                                                                      restored);

    const float effective = Speed::sanitize(restored.time_scale);
    EXPECT_TRUE(Speed::is_supported(effective)) << stored;
    EXPECT_GE(effective, Speed::k_min) << stored;
    EXPECT_LE(effective, Speed::k_max) << stored;
  }
}

} // namespace
