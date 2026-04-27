#include "game/map/visibility_service.h"
#include "render/ground/linear_feature_visibility.h"
#include <QVector3D>
#include <gtest/gtest.h>

namespace {

auto make_snapshot(int width,
                   int height) -> Game::Map::VisibilityService::Snapshot {
  Game::Map::VisibilityService::Snapshot snapshot;
  snapshot.initialized = true;
  snapshot.width = width;
  snapshot.height = height;
  snapshot.tile_size = 1.0F;
  snapshot.half_width = static_cast<float>(width) * 0.5F - 0.5F;
  snapshot.half_height = static_cast<float>(height) * 0.5F - 0.5F;
  snapshot.cells.assign(
      static_cast<std::size_t>(width * height),
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Unseen));
  return snapshot;
}

TEST(LinearFeatureVisibilityTest, DefaultsToVisibleWithoutSnapshot) {
  const auto result = Render::Ground::evaluate_linear_feature_visibility(
      nullptr, QVector3D(-1.0F, 0.0F, 0.0F), QVector3D(1.0F, 0.0F, 0.0F));

  EXPECT_TRUE(result.visible);
  EXPECT_FLOAT_EQ(result.alpha, 1.0F);
  EXPECT_EQ(result.color_multiplier, QVector3D(1.0F, 1.0F, 1.0F));
}

TEST(LinearFeatureVisibilityTest, HidesUnseenSegments) {
  const auto snapshot = make_snapshot(5, 5);

  const auto result = Render::Ground::evaluate_linear_feature_visibility(
      &snapshot, QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 0.0F, 0.0F));

  EXPECT_FALSE(result.visible);
}

TEST(LinearFeatureVisibilityTest, DimsExploredSegments) {
  auto snapshot = make_snapshot(5, 5);
  snapshot.cells[static_cast<std::size_t>(2 * snapshot.width + 2)] =
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Explored);

  const auto result = Render::Ground::evaluate_linear_feature_visibility(
      &snapshot, QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 0.0F, 0.0F));

  EXPECT_TRUE(result.visible);
  EXPECT_FLOAT_EQ(result.alpha, 0.5F);
  EXPECT_EQ(result.color_multiplier, QVector3D(0.4F, 0.4F, 0.45F));
}

TEST(LinearFeatureVisibilityTest, AllowsOutOfBoundsOverride) {
  const auto snapshot = make_snapshot(5, 5);
  Render::Ground::LinearFeatureVisibilityOptions options;
  options.treat_out_of_bounds_as_visible = true;

  const auto result = Render::Ground::evaluate_linear_feature_visibility(
      &snapshot, QVector3D(50.0F, 0.0F, 50.0F), QVector3D(55.0F, 0.0F, 55.0F),
      options);

  EXPECT_TRUE(result.visible);
  EXPECT_FLOAT_EQ(result.alpha, 1.0F);
}

} // namespace
