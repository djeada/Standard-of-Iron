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
  EXPECT_NEAR(field.sample_slope_at(2, 3), 0.0F, 1e-6F);
  EXPECT_NEAR(field.sample_curvature_at(2, 3), 0.0F, 1e-6F);
}

TEST_F(TerrainServiceTest, DerivedFieldCapturesSlopeAndCurvature) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 16;
  map_def.grid.height = 16;
  map_def.grid.tile_size = 1.0F;
  map_def.terrain.push_back(
      {Game::Map::TerrainType::Hill, 0.0F, 0.0F, 4.0F, 8.0F, 8.0F, 3.0F});

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  const auto &field = terrain.terrain_field();

  ASSERT_FALSE(field.empty());
  EXPECT_TRUE(std::any_of(field.slopes.begin(), field.slopes.end(),
                          [](float slope) { return slope > 0.05F; }));
  EXPECT_TRUE(
      std::any_of(field.curvature.begin(), field.curvature.end(),
                  [](float curvature) { return std::abs(curvature) > 0.01F; }));
}

TEST_F(TerrainServiceTest, HillEntrancesCarveLowerCenterPathThanShoulders) {
  Game::Map::TerrainHeightMap height_map(41, 41, 1.0F);
  Game::Map::TerrainFeature hill{
      .type = Game::Map::TerrainType::Hill,
      .center_x = 0.0F,
      .center_z = 0.0F,
      .width = 14.0F,
      .depth = 14.0F,
      .height = 5.0F,
  };
  hill.entrances.push_back(QVector3D(0.0F, 0.0F, -7.0F));

  height_map.buildFromFeatures({hill});

  constexpr int kCenterX = 20;
  EXPECT_TRUE(height_map.isHillEntrance(kCenterX, 15));
  EXPECT_TRUE(height_map.is_walkable(kCenterX, 15));
  EXPECT_TRUE(height_map.is_walkable(kCenterX, 16));
  EXPECT_LT(height_map.getHeightAtGrid(kCenterX, 15),
            height_map.getHeightAtGrid(kCenterX - 2, 15));
  EXPECT_LT(height_map.getHeightAtGrid(kCenterX, 16),
            height_map.getHeightAtGrid(kCenterX - 2, 16));
  EXPECT_LT(height_map.getHeightAtGrid(kCenterX, 14),
            height_map.getHeightAtGrid(kCenterX, 15));
  EXPECT_LT(height_map.getHeightAtGrid(kCenterX, 15),
            height_map.getHeightAtGrid(kCenterX, 16));
}

TEST_F(TerrainServiceTest, RestoringTerrainRebuildsDerivedField) {
  std::vector<float> heights = {0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                2.0F, 0.0F, 2.0F, 4.0F};
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

TEST_F(TerrainServiceTest, SurfaceHeightResolverUsesFallbackWhenUninitialized) {
  auto &terrain = Game::Map::TerrainService::instance();

  auto const sample = terrain.sample_surface_height(3.0F, -2.0F, 7.5F);

  EXPECT_FLOAT_EQ(sample.world_y, 7.5F);
  EXPECT_EQ(sample.kind, Game::Map::SurfaceHeightKind::Fallback);
  EXPECT_FLOAT_EQ(terrain.resolve_surface_world_y(3.0F, -2.0F, 0.25F, 7.5F),
                  7.75F);
  EXPECT_EQ(terrain.resolve_surface_world_position(3.0F, -2.0F, 0.25F, 7.5F),
            QVector3D(3.0F, 7.75F, -2.0F));
}

TEST_F(TerrainServiceTest, SurfaceHeightResolverMarksRoadSurface) {
  std::vector<float> heights(25, 1.0F);
  std::vector<Game::Map::TerrainType> terrain_types(
      heights.size(), Game::Map::TerrainType::Flat);
  std::vector<Game::Map::RoadSegment> roads{
      {.start = QVector3D(-2.0F, 0.0F, 0.0F),
       .end = QVector3D(2.0F, 0.0F, 0.0F),
       .width = 2.0F}};

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.restore_from_serialized(5, 5, 1.0F, heights, terrain_types, {}, roads,
                                  {}, {});

  auto const sample = terrain.sample_surface_height(0.0F, 0.0F);

  EXPECT_NEAR(sample.world_y, 1.02F, 0.0001F);
  EXPECT_EQ(sample.kind, Game::Map::SurfaceHeightKind::Road);
  EXPECT_NEAR(terrain.resolve_surface_world_y(0.0F, 0.0F, 0.30F, 0.0F), 1.32F,
              0.0001F);
  EXPECT_EQ(terrain.resolve_surface_world_position(0.5F, -0.5F, 0.30F, 0.0F),
            QVector3D(0.5F, 1.32F, -0.5F));
}

TEST_F(TerrainServiceTest, SurfaceHeightResolverPrefersBridgeDeckOverRoad) {
  std::vector<float> heights(81, 1.0F);
  std::vector<Game::Map::TerrainType> terrain_types(
      heights.size(), Game::Map::TerrainType::Flat);
  std::vector<Game::Map::RoadSegment> roads{
      {.start = QVector3D(-3.0F, 0.0F, 0.0F),
       .end = QVector3D(3.0F, 0.0F, 0.0F),
       .width = 2.0F}};
  std::vector<Game::Map::Bridge> bridges{{.start = QVector3D(-2.0F, 1.0F, 0.0F),
                                          .end = QVector3D(2.0F, 1.0F, 0.0F),
                                          .width = 2.0F,
                                          .height = 0.6F}};

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.restore_from_serialized(9, 9, 1.0F, heights, terrain_types, {}, roads,
                                  bridges, {});

  auto const sample = terrain.sample_surface_height(0.0F, 0.0F);

  EXPECT_NEAR(sample.world_y, 1.744F, 0.0001F);
  EXPECT_EQ(sample.kind, Game::Map::SurfaceHeightKind::Bridge);
  EXPECT_NEAR(terrain.resolve_surface_world_y(0.0F, 0.0F, 0.25F, 0.0F), 1.994F,
              0.0001F);
}

TEST_F(TerrainServiceTest, HillSlopeFormsSmoothGradientBeyondPlateau) {

  Game::Map::TerrainHeightMap height_map(61, 61, 1.0F);
  Game::Map::TerrainFeature hill{
      .type = Game::Map::TerrainType::Hill,
      .center_x = 0.0F,
      .center_z = 0.0F,
      .width = 20.0F,
      .depth = 20.0F,
      .height = 4.0F,
  };
  height_map.buildFromFeatures({hill});

  float const h_near = height_map.getHeightAtGrid(30, 19);
  float const h_mid = height_map.getHeightAtGrid(30, 15);
  float const h_far = height_map.getHeightAtGrid(30, 12);

  EXPECT_GT(h_near, 0.0F);
  EXPECT_GT(h_mid, 0.0F);
  EXPECT_GT(h_far, 0.0F);
  EXPECT_GT(h_near, h_mid);
  EXPECT_GT(h_mid, h_far);
}

TEST_F(TerrainServiceTest, HillSlopeHeightsHaveOrganicVariation) {

  Game::Map::TerrainHeightMap height_map(61, 61, 1.0F);
  Game::Map::TerrainFeature hill{
      .type = Game::Map::TerrainType::Hill,
      .center_x = 0.0F,
      .center_z = 0.0F,
      .width = 20.0F,
      .depth = 20.0F,
      .height = 4.0F,
  };
  height_map.buildFromFeatures({hill});

  float const h_n = height_map.getHeightAtGrid(30, 19);
  float const h_e = height_map.getHeightAtGrid(41, 30);
  float const h_s = height_map.getHeightAtGrid(30, 41);
  float const h_w = height_map.getHeightAtGrid(19, 30);

  EXPECT_GT(h_n, 0.0F);
  EXPECT_GT(h_e, 0.0F);
  EXPECT_GT(h_s, 0.0F);
  EXPECT_GT(h_w, 0.0F);

  EXPECT_NE(h_n, h_e);
  EXPECT_NE(h_n, h_s);
  EXPECT_NE(h_n, h_w);
  EXPECT_NE(h_e, h_s);
  EXPECT_NE(h_e, h_w);
  EXPECT_NE(h_s, h_w);
}

} // namespace
