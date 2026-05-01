#include "map/minimap/minimap_utils.h"
#include <cmath>
#include <gtest/gtest.h>

using namespace Game::Map::Minimap;

class MinimapUtilsTest : public ::testing::Test {
protected:
  static constexpr float EPSILON = 0.001F;
  static constexpr float TILE_SIZE = 2.0F;

  void ExpectNearPair(const std::pair<float, float> &actual,
                      const std::pair<float, float> &expected) {
    EXPECT_NEAR(actual.first, expected.first, EPSILON);
    EXPECT_NEAR(actual.second, expected.second, EPSILON);
  }
};

TEST_F(MinimapUtilsTest, PixelToWorldInversesWorldToPixel) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  std::vector<std::pair<float, float>> test_coords = {
      {0.0F, 0.0F},   {20.0F, 0.0F},  {-20.0F, 0.0F},  {0.0F, 20.0F},
      {0.0F, -20.0F}, {40.0F, 40.0F}, {-30.0F, 60.0F},
  };

  for (const auto &[world_x, world_z] : test_coords) {

    const float grid_x = world_x / TILE_SIZE;
    const float grid_z = world_z / TILE_SIZE;

    auto [px, py] = world_to_pixel(grid_x, grid_z, world_width, world_height,
                                   img_width, img_height);

    auto [result_x, result_z] = pixel_to_world(
        px, py, world_width, world_height, img_width, img_height, TILE_SIZE);

    ExpectNearPair({result_x, result_z}, {world_x, world_z});
  }
}

TEST_F(MinimapUtilsTest, CenterPixelMapsToCenterWorld) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  const float center_px = img_width / 2.0F;
  const float center_py = img_height / 2.0F;

  auto [world_x, world_z] =
      pixel_to_world(center_px, center_py, world_width, world_height, img_width,
                     img_height, TILE_SIZE);

  ExpectNearPair({world_x, world_z}, {0.0F, 0.0F});
}

TEST_F(MinimapUtilsTest, CornerPixelsMapToExpectedWorldCoords) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  auto [tl_x, tl_z] = pixel_to_world(0.0F, 0.0F, world_width, world_height,
                                     img_width, img_height, TILE_SIZE);

  auto [br_x, br_z] =
      pixel_to_world(img_width, img_height, world_width, world_height,
                     img_width, img_height, TILE_SIZE);

  const float tl_dist = std::sqrt(tl_x * tl_x + tl_z * tl_z);
  const float br_dist = std::sqrt(br_x * br_x + br_z * br_z);

  EXPECT_NEAR(tl_dist, br_dist, EPSILON);
}

TEST_F(MinimapUtilsTest, SquareMapSymmetry) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  const float quarter_x = img_width / 4.0F;
  const float quarter_y = img_height / 4.0F;
  const float three_quarter_x = 3.0F * img_width / 4.0F;
  const float three_quarter_y = 3.0F * img_height / 4.0F;

  auto [w1_x, w1_z] =
      pixel_to_world(quarter_x, quarter_y, world_width, world_height, img_width,
                     img_height, TILE_SIZE);
  auto [w2_x, w2_z] =
      pixel_to_world(three_quarter_x, three_quarter_y, world_width,
                     world_height, img_width, img_height, TILE_SIZE);

  const float g1_x = w1_x / TILE_SIZE;
  const float g1_z = w1_z / TILE_SIZE;
  auto [p1_x, p1_y] = world_to_pixel(g1_x, g1_z, world_width, world_height,
                                     img_width, img_height);
  ExpectNearPair({p1_x, p1_y}, {quarter_x, quarter_y});

  const float g2_x = w2_x / TILE_SIZE;
  const float g2_z = w2_z / TILE_SIZE;
  auto [p2_x, p2_y] = world_to_pixel(g2_x, g2_z, world_width, world_height,
                                     img_width, img_height);
  ExpectNearPair({p2_x, p2_y}, {three_quarter_x, three_quarter_y});
}

TEST_F(MinimapUtilsTest, TileSizeScaling) {
  const float world_width = 50.0F;
  const float world_height = 50.0F;
  const float img_width = 128.0F;
  const float img_height = 128.0F;
  const float tile_size = 3.0F;

  const float grid_x = 10.0F;
  const float grid_z = 10.0F;
  const float expected_world_x = grid_x * tile_size;
  const float expected_world_z = grid_z * tile_size;

  auto [px, py] = world_to_pixel(grid_x, grid_z, world_width, world_height,
                                 img_width, img_height);

  auto [world_x, world_z] = pixel_to_world(px, py, world_width, world_height,
                                           img_width, img_height, tile_size);

  ExpectNearPair({world_x, world_z}, {expected_world_x, expected_world_z});
}
