#include <gtest/gtest.h>

#include "game/map/terrain.h"

namespace {

auto measure_crown_half_depth(float width, float depth, float height) -> int {
  constexpr int k_grid = 420;
  Game::Map::TerrainHeightMap height_map(k_grid, k_grid, 1.0F);

  Game::Map::TerrainFeature hill{};
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = width;
  hill.depth = depth;
  hill.height = height;

  hill.entrances.emplace_back(-width * 0.5F, 0.0F, 0.0F);
  hill.entrances.emplace_back(width * 0.5F, 0.0F, 0.0F);
  height_map.build_from_features({hill});

  const int mid = k_grid / 2;
  int extent = 0;
  while (extent < k_grid / 2 - 2 && height_map.is_walkable(mid, mid + extent + 1)) {
    ++extent;
  }
  return extent;
}

} // namespace

TEST(HillCrownGeometryTest, CrownHalfExtentTracksAuthoredDimension) {
  struct Case {
    float width;
    float depth;
    float height;
  };

  constexpr Case k_cases[] = {
      {60.0F, 50.0F, 3.0F},
      {100.0F, 80.0F, 3.0F},
      {140.0F, 110.0F, 3.0F},
      {200.0F, 160.0F, 3.0F},
      {260.0F, 200.0F, 4.0F},
  };

  for (const auto& test_case : k_cases) {
    const int measured =
        measure_crown_half_depth(test_case.width, test_case.depth, test_case.height);
    const float ratio = static_cast<float>(measured) / test_case.depth;

    EXPECT_GT(ratio, 0.17F) << "crown shrank for depth " << test_case.depth
                            << "; the generator's 0.21 sizing rule would strand "
                               "buildings on the slope";
    EXPECT_LT(ratio, 0.26F) << "crown grew for depth " << test_case.depth
                            << "; the generator is now sizing hills larger than "
                               "they need to be";
  }
}
