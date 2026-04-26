#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

namespace {

class TerrainServiceTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

TEST_F(TerrainServiceTest, BuildsDerivedFieldForFlatTerrain) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 6;
  map_def.grid.height = 5;
  map_def.grid.tile_size = 1.5F;

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  const auto &field = terrain.terrain_field();

  ASSERT_FALSE(field.empty());
  EXPECT_EQ(field.width, 6);
  EXPECT_EQ(field.height, 5);
  EXPECT_FLOAT_EQ(field.tile_size, 1.5F);
  EXPECT_EQ(field.heights.size(), 30U);
  EXPECT_EQ(field.slopes.size(), 30U);
  EXPECT_EQ(field.curvature.size(), 30U);
  EXPECT_FLOAT_EQ(field.sample_height_at(2.0F, 3.0F), 0.0F);
  EXPECT_FLOAT_EQ(field.sample_slope_at(2, 3), 0.0F);
  EXPECT_FLOAT_EQ(field.sample_curvature_at(2, 3), 0.0F);
}

TEST_F(TerrainServiceTest, DerivedFieldCapturesSlopeAndCurvature) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 16;
  map_def.grid.height = 16;
  map_def.grid.tile_size = 1.0F;
  map_def.terrain.push_back({Game::Map::TerrainType::Hill, 0.0F, 0.0F, 4.0F,
                             8.0F, 8.0F, 3.0F});

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  const auto &field = terrain.terrain_field();

  ASSERT_FALSE(field.empty());
  EXPECT_TRUE(std::any_of(field.slopes.begin(), field.slopes.end(),
                          [](float slope) { return slope > 0.05F; }));
  EXPECT_TRUE(std::any_of(field.curvature.begin(), field.curvature.end(),
                          [](float curvature) {
                            return std::abs(curvature) > 0.01F;
                          }));
}

TEST_F(TerrainServiceTest, RestoringTerrainRebuildsDerivedField) {
  std::vector<float> heights = {
      0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 2.0F, 0.0F, 2.0F, 4.0F};
  std::vector<Game::Map::TerrainType> terrain_types(
      heights.size(), Game::Map::TerrainType::Hill);

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.restore_from_serialized(3, 3, 1.0F, heights, terrain_types, {}, {},
                                  {}, {});

  const auto &field = terrain.terrain_field();

  ASSERT_FALSE(field.empty());
  EXPECT_EQ(field.heights, heights);
  EXPECT_GT(field.sample_slope_at(1, 1), 0.0F);
  EXPECT_NE(field.sample_curvature_at(1, 1), 0.0F);
}

} // namespace
