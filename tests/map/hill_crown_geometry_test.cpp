#include <gtest/gtest.h>
#include <vector>

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

namespace {

auto build_stacked_hills(bool with_inner_hill) -> Game::Map::TerrainHeightMap {
  constexpr int k_grid = 420;
  Game::Map::TerrainHeightMap height_map(k_grid, k_grid, 1.0F);

  Game::Map::TerrainFeature outer{};
  outer.type = Game::Map::TerrainType::Hill;
  outer.width = 150.0F;
  outer.depth = 130.0F;
  outer.height = 2.6F;
  outer.crown = 0.85F;
  outer.entrances.emplace_back(-75.0F, 0.0F, 0.0F);
  outer.entrances.emplace_back(75.0F, 0.0F, 0.0F);

  Game::Map::TerrainFeature inner{};
  inner.type = Game::Map::TerrainType::Hill;
  inner.width = 56.0F;
  inner.depth = 48.0F;
  inner.height = 10.5F;
  inner.crown = 0.6F;

  std::vector<Game::Map::TerrainFeature> features{outer};
  if (with_inner_hill) {
    features.push_back(inner);
  }
  height_map.build_from_features(features);
  return height_map;
}

} // namespace

TEST(HillCrownGeometryTest, AHillRaisedOnAnotherHillNeverCarvesBelowTheOuterCrown) {
  const auto outer_only = build_stacked_hills(false);
  const auto stacked = build_stacked_hills(true);

  const int mid = outer_only.get_width() / 2;
  float lowest_drop = 0.0F;
  int lowest_x = 0;
  int lowest_z = 0;
  for (int z = 0; z < outer_only.get_height(); ++z) {
    for (int x = 0; x < outer_only.get_width(); ++x) {
      const float drop =
          outer_only.get_height_at_grid(x, z) - stacked.get_height_at_grid(x, z);
      if (drop > lowest_drop) {
        lowest_drop = drop;
        lowest_x = x;
        lowest_z = z;
      }
    }
  }
  EXPECT_LT(lowest_drop, 0.05F)
      << "the inner hill's entrance ramp dug " << lowest_drop
      << " below the outer crown at (" << lowest_x << ", " << lowest_z
      << "); a terrace fort's lower ring would stand in a trench";

  EXPECT_GT(stacked.get_height_at_grid(mid, mid),
            outer_only.get_height_at_grid(mid, mid) + 4.0F)
      << "the inner hill should stand as a keep above the outer crown";
  EXPECT_TRUE(stacked.is_walkable(mid, mid));
  EXPECT_TRUE(stacked.is_walkable(mid - 50, mid));
}
