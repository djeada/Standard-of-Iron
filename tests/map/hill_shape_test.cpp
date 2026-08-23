#include <QVector3D>

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "game/map/hill_shape.h"
#include "game/map/terrain.h"

namespace {

constexpr int k_grid = 64;
constexpr float k_tile = 1.0F;
constexpr float k_grid_half = k_grid * 0.5F - 0.5F;

auto world_from_cell(float cell) -> float {
  return (cell - k_grid_half) * k_tile;
}

auto build(const std::vector<Game::Map::TerrainFeature>& features)
    -> Game::Map::TerrainHeightMap {
  Game::Map::TerrainHeightMap height_map(k_grid, k_grid, k_tile);
  height_map.build_from_features(features);
  return height_map;
}

auto is_hill(const Game::Map::TerrainHeightMap& map, int x, int z) -> bool {
  return map.getTerrainType(x, z) == Game::Map::TerrainType::Hill;
}

auto hill_feature(Game::Map::HillShape shape,
                  float width,
                  float depth,
                  float thickness) -> Game::Map::TerrainFeature {
  Game::Map::TerrainFeature feature;
  feature.type = Game::Map::TerrainType::Hill;
  feature.center_x = world_from_cell(32.0F);
  feature.center_z = world_from_cell(32.0F);
  feature.width = width;
  feature.depth = depth;
  feature.radius = 0.0F;
  feature.height = 3.0F;
  feature.shape = shape;
  feature.thickness = thickness;
  return feature;
}

TEST(HillShapeTest, ParsesAuthoredNames) {
  Game::Map::HillShape shape = Game::Map::HillShape::Blob;
  EXPECT_TRUE(Game::Map::parse_hill_shape("boomerang", shape));
  EXPECT_EQ(shape, Game::Map::HillShape::Arc);
  EXPECT_TRUE(Game::Map::parse_hill_shape("Corridor", shape));
  EXPECT_EQ(shape, Game::Map::HillShape::Corridor);
  EXPECT_TRUE(Game::Map::parse_hill_shape("", shape));
  EXPECT_EQ(shape, Game::Map::HillShape::Blob);
  EXPECT_FALSE(Game::Map::parse_hill_shape("zigzag", shape));
}

TEST(HillShapeTest, NamesRoundTripThroughParse) {
  for (const Game::Map::HillShape shape : {Game::Map::HillShape::Blob,
                                           Game::Map::HillShape::Corridor,
                                           Game::Map::HillShape::Arc,
                                           Game::Map::HillShape::Elbow,
                                           Game::Map::HillShape::Ring,
                                           Game::Map::HillShape::Path,
                                           Game::Map::HillShape::Mask}) {
    Game::Map::HillShape parsed = Game::Map::HillShape::Blob;
    ASSERT_TRUE(Game::Map::parse_hill_shape(Game::Map::hill_shape_name(shape), parsed));
    EXPECT_EQ(parsed, shape);
  }
}

TEST(HillShapeTest, CorridorIsLongAndThin) {
  auto feature = hill_feature(Game::Map::HillShape::Corridor, 40.0F, 8.0F, 0.0F);
  feature.entrances.emplace_back(world_from_cell(32.0F), 0.0F, world_from_cell(41.0F));
  const auto map = build({feature});

  EXPECT_TRUE(is_hill(map, 32, 32));
  EXPECT_TRUE(is_hill(map, 32 - 15, 32));
  EXPECT_TRUE(is_hill(map, 32 + 15, 32));
  EXPECT_FALSE(is_hill(map, 32, 32 - 7));
  EXPECT_FALSE(is_hill(map, 32 + 25, 32));
}

TEST(HillShapeTest, CorridorFollowsRotation) {
  auto feature = hill_feature(Game::Map::HillShape::Corridor, 40.0F, 8.0F, 0.0F);
  feature.rotation_deg = 90.0F;
  feature.entrances.emplace_back(world_from_cell(41.0F), 0.0F, world_from_cell(32.0F));
  const auto map = build({feature});

  EXPECT_TRUE(is_hill(map, 32, 32 - 15));
  EXPECT_TRUE(is_hill(map, 32, 32 + 15));
  EXPECT_FALSE(is_hill(map, 32 - 7, 32));
}

TEST(HillShapeTest, CorridorRunsAlongItsLongAxisWhenDepthDominates) {
  auto feature = hill_feature(Game::Map::HillShape::Corridor, 8.0F, 40.0F, 0.0F);
  feature.entrances.emplace_back(world_from_cell(41.0F), 0.0F, world_from_cell(32.0F));
  const auto map = build({feature});

  EXPECT_TRUE(is_hill(map, 32, 32 - 15));
  EXPECT_TRUE(is_hill(map, 32, 32 + 15));
  EXPECT_FALSE(is_hill(map, 32 - 7, 32));
}

TEST(HillShapeTest, RingLeavesAHollowInterior) {
  const auto map =
      build({hill_feature(Game::Map::HillShape::Ring, 40.0F, 40.0F, 10.0F)});

  EXPECT_FALSE(is_hill(map, 32, 32));
  EXPECT_TRUE(is_hill(map, 32 + 15, 32));
  EXPECT_TRUE(is_hill(map, 32 - 15, 32));
  EXPECT_TRUE(is_hill(map, 32, 32 + 15));
  EXPECT_TRUE(is_hill(map, 32, 32 - 15));
}

TEST(HillShapeTest, ArcWrapsOnlyItsSweep) {
  auto feature = hill_feature(Game::Map::HillShape::Arc, 40.0F, 40.0F, 10.0F);
  feature.has_sweep = true;
  feature.sweep_degrees = 90.0F;
  const auto map = build({feature});

  EXPECT_TRUE(is_hill(map, 32 + 15, 32));
  EXPECT_FALSE(is_hill(map, 32 - 15, 32));
  EXPECT_FALSE(is_hill(map, 32, 32));
}

TEST(HillShapeTest, ElbowCoversTwoArmsAndNotTheOppositeCorner) {
  auto feature = hill_feature(Game::Map::HillShape::Elbow, 36.0F, 36.0F, 8.0F);
  const auto map = build({feature});

  EXPECT_TRUE(is_hill(map, 32 - 14, 32 - 14));
  EXPECT_TRUE(is_hill(map, 32 + 10, 32 - 14));
  EXPECT_TRUE(is_hill(map, 32 - 14, 32 + 10));
  EXPECT_FALSE(is_hill(map, 32 + 12, 32 + 12));
}

TEST(HillShapeTest, PaintedMaskFollowsTheAuthoredCells) {
  Game::Map::TerrainFeature feature =
      hill_feature(Game::Map::HillShape::Mask, 0.0F, 0.0F, 0.0F);
  for (int z = 24; z <= 40; ++z) {
    for (int x = 24; x <= 40; ++x) {
      const bool in_column = x >= 30 && x <= 34;
      const bool in_row = z >= 30 && z <= 34;
      if (in_column || in_row) {
        feature.mask_cells.emplace_back(
            world_from_cell(float(x)), 0.0F, world_from_cell(float(z)));
      }
    }
  }

  const auto map = build({feature});

  EXPECT_TRUE(is_hill(map, 32, 32));
  EXPECT_TRUE(is_hill(map, 32, 26));
  EXPECT_TRUE(is_hill(map, 26, 32));
  EXPECT_TRUE(is_hill(map, 38, 32));
  EXPECT_FALSE(is_hill(map, 26, 26));
  EXPECT_FALSE(is_hill(map, 38, 38));
  EXPECT_FALSE(is_hill(map, 32, 44));
}

TEST(HillShapeTest, PathFollowsAuthoredPoints) {
  Game::Map::TerrainFeature feature =
      hill_feature(Game::Map::HillShape::Path, 40.0F, 40.0F, 8.0F);
  feature.shape_points.emplace_back(
      world_from_cell(20.0F), 0.0F, world_from_cell(20.0F));
  feature.shape_points.emplace_back(
      world_from_cell(32.0F), 0.0F, world_from_cell(26.0F));
  feature.shape_points.emplace_back(
      world_from_cell(44.0F), 0.0F, world_from_cell(44.0F));
  feature.entrances.emplace_back(world_from_cell(14.0F), 0.0F, world_from_cell(26.0F));

  const auto map = build({feature});

  EXPECT_TRUE(is_hill(map, 32, 26));
  EXPECT_TRUE(is_hill(map, 22, 21));
  EXPECT_FALSE(is_hill(map, 20, 44));
  EXPECT_FALSE(is_hill(map, 44, 20));
}

TEST(HillShapeTest, AnEntranceOnTheCrownStillCarvesARamp) {
  auto feature = hill_feature(Game::Map::HillShape::Ring, 40.0F, 40.0F, 10.0F);
  feature.entrances.emplace_back(world_from_cell(47.0F), 0.0F, world_from_cell(32.0F));
  const auto map = build({feature});

  bool walkable_band = false;
  for (int x = 40; x <= 50; ++x) {
    if (map.is_walkable(x, 32)) {
      walkable_band = true;
      break;
    }
  }
  EXPECT_TRUE(walkable_band);
}

TEST(HillShapeTest, BlobStaysTheLegacyEllipse) {
  Game::Map::TerrainFeature shaped =
      hill_feature(Game::Map::HillShape::Blob, 30.0F, 18.0F, 0.0F);
  Game::Map::TerrainFeature legacy = shaped;
  legacy.shape = Game::Map::HillShape::Blob;

  const auto shaped_map = build({shaped});
  const auto legacy_map = build({legacy});

  for (int z = 0; z < k_grid; ++z) {
    for (int x = 0; x < k_grid; ++x) {
      ASSERT_FLOAT_EQ(shaped_map.get_height_at_grid(x, z),
                      legacy_map.get_height_at_grid(x, z))
          << "cell " << x << "," << z;
    }
  }
}

TEST(HillShapeTest, DegenerateShapeFallsBackToBlob) {
  Game::Map::HillShapeParams params;
  params.shape = Game::Map::HillShape::Path;
  params.half_width_cells = 10.0F;
  params.half_depth_cells = 10.0F;
  const auto geometry = Game::Map::build_hill_shape(params);

  EXPECT_EQ(geometry.shape, Game::Map::HillShape::Blob);
  EXPECT_FALSE(geometry.is_shaped());
}

TEST(HillShapeTest, ShapedHillsStillCarveAnEntrance) {
  auto feature = hill_feature(Game::Map::HillShape::Corridor, 40.0F, 10.0F, 0.0F);
  feature.entrances.emplace_back(world_from_cell(32.0F), 0.0F, world_from_cell(24.0F));
  const auto map = build({feature});

  bool walkable_crown = false;
  for (int x = 26; x <= 38; ++x) {
    if (map.is_walkable(x, 32)) {
      walkable_crown = true;
      break;
    }
  }
  EXPECT_TRUE(walkable_crown);
}

} // namespace
