#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <utility>

namespace {

class TerrainServiceTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

TEST_F(TerrainServiceTest, BuildsDerivedFieldForFlatTerrainWithIrregularity) {
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
  EXPECT_TRUE(std::any_of(field.heights.begin(), field.heights.end(),
                          [](float height) { return height > 0.005F; }));
  EXPECT_TRUE(std::any_of(field.slopes.begin(), field.slopes.end(),
                          [](float slope) { return slope > 0.0005F; }));
  EXPECT_TRUE(std::any_of(
      field.curvature.begin(), field.curvature.end(),
      [](float curvature) { return std::abs(curvature) > 0.0001F; }));
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

  height_map.build_from_features({hill});

  constexpr int k_center_x = 20;
  EXPECT_TRUE(height_map.isHillEntrance(k_center_x, 15));
  EXPECT_TRUE(height_map.is_walkable(k_center_x, 15));
  EXPECT_TRUE(height_map.is_walkable(k_center_x, 16));
  EXPECT_LT(height_map.get_height_at_grid(k_center_x, 15),
            height_map.get_height_at_grid(k_center_x - 2, 15));
  EXPECT_LT(height_map.get_height_at_grid(k_center_x, 16),
            height_map.get_height_at_grid(k_center_x - 2, 16));
  EXPECT_LT(height_map.get_height_at_grid(k_center_x, 14),
            height_map.get_height_at_grid(k_center_x, 15));
  EXPECT_LT(height_map.get_height_at_grid(k_center_x, 15),
            height_map.get_height_at_grid(k_center_x, 16));
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

TEST_F(TerrainServiceTest, RestoringTerrainUpdatesScatterSources) {
  Game::Map::MapDefinition map_def;
  map_def.world_props.push_back({.type = Game::Map::WorldProp::Type::FireCamp,
                                 .x = 1.0F,
                                 .z = 2.0F,
                                 .intensity = 1.1F,
                                 .radius = 3.5F,
                                 .persistent = true});
  map_def.world_props.push_back({.type = Game::Map::WorldProp::Type::Boulder,
                                 .x = 3.0F,
                                 .z = 4.0F,
                                 .scale = 1.25F,
                                 .rotation = 0.75F});

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  std::vector<float> heights(9, 0.0F);
  std::vector<Game::Map::TerrainType> terrain_types(
      heights.size(), Game::Map::TerrainType::Flat);
  std::vector<Game::Map::WorldProp> restored_world_props{
      {.type = Game::Map::WorldProp::Type::FireCamp,
       .x = 5.0F,
       .z = 6.0F,
       .intensity = 0.8F,
       .radius = 2.5F,
       .persistent = false},
      {.type = Game::Map::WorldProp::Type::Tent,
       .x = 7.0F,
       .z = 8.0F,
       .scale = 1.5F,
       .rotation = 1.2F}};

  terrain.restore_from_serialized(3, 3, 1.0F, heights, terrain_types, {}, {},
                                  {}, {}, restored_world_props);

  ASSERT_EQ(terrain.world_props().size(), 2U);
  EXPECT_EQ(terrain.world_props().front().type,
            Game::Map::WorldProp::Type::FireCamp);
  EXPECT_FLOAT_EQ(terrain.world_props().front().x, 5.0F);
  EXPECT_FALSE(terrain.world_props().front().persistent);
  EXPECT_EQ(terrain.world_props().back().type,
            Game::Map::WorldProp::Type::Tent);
  EXPECT_FLOAT_EQ(terrain.world_props().back().rotation, 1.2F);
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
  float const road_surface_y = Game::Map::road_surface_world_y(1.0F);

  EXPECT_NEAR(sample.world_y, road_surface_y, 0.0001F);
  EXPECT_EQ(sample.kind, Game::Map::SurfaceHeightKind::Road);
  EXPECT_NEAR(terrain.resolve_surface_world_y(0.0F, 0.0F, 0.30F, 0.0F),
              road_surface_y + 0.30F, 0.0001F);
  EXPECT_EQ(terrain.resolve_surface_world_position(0.5F, -0.5F, 0.30F, 0.0F),
            QVector3D(0.5F, road_surface_y + 0.30F, -0.5F));
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
  float const bridge_surface_y =
      Game::Map::bridge_deck_world_y(bridges.front(), 0.5F);

  EXPECT_NEAR(sample.world_y, bridge_surface_y, 0.0001F);
  EXPECT_EQ(sample.kind, Game::Map::SurfaceHeightKind::Bridge);
  EXPECT_NEAR(terrain.resolve_surface_world_y(0.0F, 0.0F, 0.25F, 0.0F),
              bridge_surface_y + 0.25F, 0.0001F);
}

TEST_F(TerrainServiceTest,
       BridgeWalkabilityRespectsBridgeWidthWhenTileSizeExceedsOne) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 9;
  map_def.grid.height = 9;
  map_def.grid.tile_size = 2.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -8.0F), QVector3D(0.0F, 0.0F, 8.0F), 2.0F});
  map_def.bridges.push_back(
      {QVector3D(-4.0F, 0.0F, 0.0F), QVector3D(4.0F, 0.0F, 0.0F), 2.0F, 0.6F});

  auto &terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  EXPECT_TRUE(terrain.is_on_bridge(0.0F, 0.0F));
  EXPECT_FALSE(terrain.is_on_bridge(0.0F, 2.0F));

  EXPECT_TRUE(terrain.is_walkable(4, 4));
  EXPECT_FALSE(terrain.is_walkable(4, 5));
}

TEST_F(TerrainServiceTest, HillFootprintStaysInsidePlateauBounds) {
  Game::Map::TerrainHeightMap height_map(61, 61, 1.0F);
  Game::Map::TerrainFeature hill{
      .type = Game::Map::TerrainType::Hill,
      .center_x = 0.0F,
      .center_z = 0.0F,
      .width = 20.0F,
      .depth = 20.0F,
      .height = 4.0F,
  };
  height_map.build_from_features({hill});

  for (auto const [x, z] : {std::pair{30, 19}, std::pair{41, 30},
                            std::pair{30, 41}, std::pair{19, 30}}) {
    EXPECT_FLOAT_EQ(height_map.get_height_at_grid(x, z), 0.0F);
    EXPECT_EQ(height_map.getTerrainType(x, z), Game::Map::TerrainType::Flat);
    EXPECT_TRUE(height_map.is_walkable(x, z));
  }
}

} // namespace
