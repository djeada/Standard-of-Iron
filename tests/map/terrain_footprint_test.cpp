#include <cmath>
#include <gtest/gtest.h>

#include "game/map/terrain.h"
#include "game/map/terrain_footprint.h"

namespace {

constexpr int k_grid = 420;

auto build_map(const Game::Map::TerrainFeature& feature)
    -> Game::Map::TerrainHeightMap {
  Game::Map::TerrainHeightMap height_map(k_grid, k_grid, 1.0F);
  height_map.build_from_features({feature});
  return height_map;
}

auto measured_half_extent(const Game::Map::TerrainHeightMap& height_map,
                          Game::Map::TerrainType type,
                          int step_x,
                          int step_z) -> int {
  const int mid = k_grid / 2;
  int extent = 0;
  while (extent < (k_grid / 2) - 2 &&
         height_map.getTerrainType(mid + (step_x * (extent + 1)),
                                   mid + (step_z * (extent + 1))) == type) {
    ++extent;
  }
  return extent;
}

auto hill_feature(float width, float depth) -> Game::Map::TerrainFeature {
  Game::Map::TerrainFeature hill{};
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = width;
  hill.depth = depth;
  hill.height = 3.0F;
  return hill;
}

} // namespace

TEST(TerrainFootprintTest, AuthoredHillExtentsAreFullWidths) {
  const auto footprint = Game::Map::hill_footprint_cells(
      {.width = 80.0F, .depth = 40.0F, .radius = 0.0F, .tile_size = 1.0F});

  EXPECT_FLOAT_EQ(footprint.half_width, 40.0F);
  EXPECT_FLOAT_EQ(footprint.half_depth, 20.0F);

  const auto height_map = build_map(hill_feature(80.0F, 40.0F));
  const int measured_x =
      measured_half_extent(height_map, Game::Map::TerrainType::Hill, 1, 0);
  const int measured_z =
      measured_half_extent(height_map, Game::Map::TerrainType::Hill, 0, 1);

  const float spread = 1.0F + footprint.organic_spread;
  EXPECT_GE(static_cast<float>(measured_x), footprint.half_width * (2.0F - spread));
  EXPECT_LE(static_cast<float>(measured_x), footprint.half_width * spread);
  EXPECT_GE(static_cast<float>(measured_z), footprint.half_depth * (2.0F - spread));
  EXPECT_LE(static_cast<float>(measured_z), footprint.half_depth * spread);
}

TEST(TerrainFootprintTest, RadiusOnlyHillsMatchTheRuntimeCircle) {
  const auto footprint = Game::Map::hill_footprint_cells(
      {.width = 0.0F, .depth = 0.0F, .radius = 18.0F, .tile_size = 1.0F});

  EXPECT_FLOAT_EQ(footprint.half_width, 18.0F);
  EXPECT_FLOAT_EQ(footprint.half_depth, 18.0F);

  Game::Map::TerrainFeature hill = hill_feature(36.0F, 36.0F);
  hill.radius = 18.0F;
  const auto height_map = build_map(hill);
  const int measured =
      measured_half_extent(height_map, Game::Map::TerrainType::Hill, 1, 0);

  const float spread = 1.0F + footprint.organic_spread;
  EXPECT_GE(static_cast<float>(measured), footprint.half_width * (2.0F - spread));
  EXPECT_LE(static_cast<float>(measured), footprint.half_width * spread);
}

TEST(TerrainFootprintTest, MountainMinorAxisIsWiderThanTheOldEditorEllipse) {
  const auto footprint = Game::Map::mountain_footprint_cells(
      {.width = 0.0F, .depth = 0.0F, .radius = 20.0F, .tile_size = 1.0F});

  EXPECT_FLOAT_EQ(footprint.half_width, std::max(20.0F * 1.38F, 26.0F));
  EXPECT_FLOAT_EQ(footprint.half_depth, std::max(20.0F * 0.55F, 5.0F));

  Game::Map::TerrainFeature mountain{};
  mountain.type = Game::Map::TerrainType::Mountain;
  mountain.center_x = 0.0F;
  mountain.center_z = 0.0F;
  mountain.radius = 20.0F;
  mountain.height = 8.0F;

  const auto height_map = build_map(mountain);
  const int measured_z =
      measured_half_extent(height_map, Game::Map::TerrainType::Mountain, 0, 1);

  EXPECT_GT(measured_z, static_cast<int>(20.0F * 0.22F))
      << "the editor used to draw mountains with a 0.22 minor axis, so roads that "
         "looked clear collided in game";
}

TEST(TerrainFootprintTest, CampaignRadiusHillsStretchAndSpinDeterministically) {
  const Game::Map::HillFootprintInput input{.width = 0.0F,
                                            .depth = 0.0F,
                                            .radius = 20.0F,
                                            .rotation_deg = 0.0F,
                                            .tile_size = 1.0F,
                                            .grid_center_x = 128.0F,
                                            .grid_center_z = 96.0F,
                                            .campaign_scale = true};

  const auto first = Game::Map::hill_footprint_cells(input);
  const auto second = Game::Map::hill_footprint_cells(input);

  EXPECT_FLOAT_EQ(first.half_width, 20.0F * 1.18F);
  EXPECT_FLOAT_EQ(first.half_depth, 20.0F);
  EXPECT_FLOAT_EQ(first.rotation_deg, second.rotation_deg);
  EXPECT_GE(first.rotation_deg, 0.0F);
  EXPECT_LE(first.rotation_deg, 180.0F);
}
