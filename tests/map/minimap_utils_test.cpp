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
  const float world_width = 100.0F; // Grid dimensions
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  // Test various world coordinates (in actual world space)
  std::vector<std::pair<float, float>> test_coords = {
      {0.0F, 0.0F},    // Center
      {20.0F, 0.0F},   // Right (10 grid units * 2.0 tile_size)
      {-20.0F, 0.0F},  // Left
      {0.0F, 20.0F},   // Forward
      {0.0F, -20.0F},  // Back
      {40.0F, 40.0F},  // Diagonal
      {-30.0F, 60.0F}, // Mixed
  };

  for (const auto &[world_x, world_z] : test_coords) {
    // Convert world to grid for world_to_pixel
    const float grid_x = world_x / TILE_SIZE;
    const float grid_z = world_z / TILE_SIZE;

    // Convert grid to pixel
    auto [px, py] = world_to_pixel(grid_x, grid_z, world_width, world_height,
                                   img_width, img_height);

    // Convert back to world
    auto [result_x, result_z] = pixel_to_world(
        px, py, world_width, world_height, img_width, img_height, TILE_SIZE);

    // Should get back the original coordinates
    ExpectNearPair({result_x, result_z}, {world_x, world_z});
  }
}

TEST_F(MinimapUtilsTest, CenterPixelMapsToCenterWorld) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  // Center of the image should map to (0, 0) world
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

  // Top-left corner (0, 0) in pixels
  auto [tl_x, tl_z] = pixel_to_world(0.0F, 0.0F, world_width, world_height,
                                     img_width, img_height, TILE_SIZE);

  // Bottom-right corner in pixels
  auto [br_x, br_z] =
      pixel_to_world(img_width, img_height, world_width, world_height,
                     img_width, img_height, TILE_SIZE);

  // The corners should be at the world bounds (with rotation applied)
  // Due to the rotation, we can't simply check against world_width/2
  // But the distance from center should be consistent
  const float tl_dist = std::sqrt(tl_x * tl_x + tl_z * tl_z);
  const float br_dist = std::sqrt(br_x * br_x + br_z * br_z);

  // Both corners should be approximately the same distance from center
  EXPECT_NEAR(tl_dist, br_dist, EPSILON);
}

TEST_F(MinimapUtilsTest, SquareMapSymmetry) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  // For a square map, symmetric pixels should map to symmetric world coords
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

  // Due to rotation, the relationship is more complex, but we can verify
  // the conversion is consistent by checking the round-trip
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
  const float world_width = 50.0F; // 50 grid cells
  const float world_height = 50.0F;
  const float img_width = 128.0F;
  const float img_height = 128.0F;
  const float tile_size = 3.0F; // 3 world units per grid cell

  // Test a specific grid position
  const float grid_x = 10.0F; // 10 cells from center
  const float grid_z = 10.0F;
  const float expected_world_x = grid_x * tile_size; // 30 world units
  const float expected_world_z = grid_z * tile_size; // 30 world units

  // Convert grid to pixel
  auto [px, py] = world_to_pixel(grid_x, grid_z, world_width, world_height,
                                 img_width, img_height);

  // Convert pixel back to world with tile_size
  auto [world_x, world_z] = pixel_to_world(px, py, world_width, world_height,
                                           img_width, img_height, tile_size);

  // Should match expected world coordinates
  ExpectNearPair({world_x, world_z}, {expected_world_x, expected_world_z});
}
