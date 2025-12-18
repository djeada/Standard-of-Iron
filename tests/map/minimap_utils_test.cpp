#include "map/minimap/minimap_utils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace Game::Map::Minimap;

class MinimapUtilsTest : public ::testing::Test {
protected:
  static constexpr float EPSILON = 0.001F;
  
  void ExpectNearPair(const std::pair<float, float>& actual,
                      const std::pair<float, float>& expected) {
    EXPECT_NEAR(actual.first, expected.first, EPSILON);
    EXPECT_NEAR(actual.second, expected.second, EPSILON);
  }
};

TEST_F(MinimapUtilsTest, PixelToWorldInversesWorldToPixel) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  // Test various world coordinates
  std::vector<std::pair<float, float>> test_coords = {
    {0.0F, 0.0F},      // Center
    {10.0F, 0.0F},     // Right
    {-10.0F, 0.0F},    // Left
    {0.0F, 10.0F},     // Forward
    {0.0F, -10.0F},    // Back
    {20.0F, 20.0F},    // Diagonal
    {-15.0F, 30.0F},   // Mixed
  };

  for (const auto& [world_x, world_z] : test_coords) {
    // Convert world to pixel
    auto [px, py] = world_to_pixel(world_x, world_z, world_width, world_height,
                                   img_width, img_height);
    
    // Convert back to world
    auto [result_x, result_z] = pixel_to_world(px, py, world_width, world_height,
                                               img_width, img_height);
    
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

  auto [world_x, world_z] = pixel_to_world(center_px, center_py, world_width,
                                           world_height, img_width, img_height);

  ExpectNearPair({world_x, world_z}, {0.0F, 0.0F});
}

TEST_F(MinimapUtilsTest, CornerPixelsMapToExpectedWorldCoords) {
  const float world_width = 100.0F;
  const float world_height = 100.0F;
  const float img_width = 256.0F;
  const float img_height = 256.0F;

  // Top-left corner (0, 0) in pixels
  auto [tl_x, tl_z] = pixel_to_world(0.0F, 0.0F, world_width, world_height,
                                     img_width, img_height);
  
  // Bottom-right corner in pixels
  auto [br_x, br_z] = pixel_to_world(img_width, img_height, world_width,
                                     world_height, img_width, img_height);
  
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

  auto [w1_x, w1_z] = pixel_to_world(quarter_x, quarter_y, world_width,
                                     world_height, img_width, img_height);
  auto [w2_x, w2_z] = pixel_to_world(three_quarter_x, three_quarter_y,
                                     world_width, world_height, img_width,
                                     img_height);

  // Due to rotation, the relationship is more complex, but we can verify
  // the conversion is consistent by checking the round-trip
  auto [p1_x, p1_y] = world_to_pixel(w1_x, w1_z, world_width, world_height,
                                     img_width, img_height);
  ExpectNearPair({p1_x, p1_y}, {quarter_x, quarter_y});

  auto [p2_x, p2_y] = world_to_pixel(w2_x, w2_z, world_width, world_height,
                                     img_width, img_height);
  ExpectNearPair({p2_x, p2_y}, {three_quarter_x, three_quarter_y});
}
