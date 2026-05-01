#include <gtest/gtest.h>

#include "render/ground/spawn_validator.h"

using namespace Render::Ground;
using namespace Game::Map;

class SpawnValidatorTest : public ::testing::Test {
protected:
  void SetUp() override {

    width = 10;
    height = 10;
    tile_size = 1.0F;

    height_data.resize(static_cast<size_t>(width * height), 0.0F);

    terrain_types.resize(static_cast<size_t>(width * height),
                         TerrainType::Flat);
  }

  void build_cache() {
    terrain_cache.build_from_height_map(height_data, terrain_types, width,
                                        height, tile_size);
  }

  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  std::vector<float> height_data;
  std::vector<TerrainType> terrain_types;
  SpawnTerrainCache terrain_cache;
};

TEST_F(SpawnValidatorTest, TerrainCacheBuildFromHeightMap) {
  build_cache();

  EXPECT_EQ(terrain_cache.width, width);
  EXPECT_EQ(terrain_cache.height, height);
  EXPECT_EQ(terrain_cache.tile_size, tile_size);
  EXPECT_FALSE(terrain_cache.normals.empty());
  EXPECT_FALSE(terrain_cache.heights.empty());
}

TEST_F(SpawnValidatorTest, TerrainCacheSampleHeightFlat) {
  build_cache();

  EXPECT_FLOAT_EQ(terrain_cache.sample_height_at(5.0F, 5.0F), 0.0F);
  EXPECT_FLOAT_EQ(terrain_cache.sample_height_at(0.0F, 0.0F), 0.0F);
  EXPECT_FLOAT_EQ(terrain_cache.sample_height_at(9.0F, 9.0F), 0.0F);
}

TEST_F(SpawnValidatorTest, TerrainCacheGetSlopeFlat) {
  build_cache();

  float slope = terrain_cache.get_slope_at(5, 5);
  EXPECT_LT(slope, 0.01F);
}

TEST_F(SpawnValidatorTest, TerrainCacheGetTerrainType) {

  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::Mountain;
  terrain_types[static_cast<size_t>(3 * width + 3)] = TerrainType::River;

  build_cache();

  EXPECT_EQ(terrain_cache.get_terrain_type_at(5, 5), TerrainType::Mountain);
  EXPECT_EQ(terrain_cache.get_terrain_type_at(3, 3), TerrainType::River);
  EXPECT_EQ(terrain_cache.get_terrain_type_at(0, 0), TerrainType::Flat);
}

TEST_F(SpawnValidatorTest, SpawnValidatorAllowsFlatTerrain) {
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;

  SpawnValidator validator(terrain_cache, config);

  EXPECT_TRUE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksMountainTerrain) {

  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::Mountain;
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.allow_mountain = false;

  SpawnValidator validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksRiverTerrain) {

  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::River;
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.allow_river = false;

  SpawnValidator validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorRiverMarginCheck) {

  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::River;
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.river_margin = 1;
  config.check_river_margin = true;

  SpawnValidator validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_grid(4.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(6.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 4.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 6.0F));

  EXPECT_TRUE(validator.can_spawn_at_grid(0.0F, 0.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorEdgePaddingCheck) {
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.2F;

  SpawnValidator validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_grid(0.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 0.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(9.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 9.0F));

  EXPECT_TRUE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, GridToWorldConversion) {
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = 2.0F;
  config.edge_padding = 0.0F;

  SpawnValidator validator(terrain_cache, config);

  float world_x = 0.0F;
  float world_z = 0.0F;

  validator.grid_to_world(5.0F, 5.0F, world_x, world_z);
  EXPECT_NEAR(world_x, 1.0F, 0.01F);
  EXPECT_NEAR(world_z, 1.0F, 0.01F);
}

TEST_F(SpawnValidatorTest, MakePlantSpawnConfigDefaults) {
  SpawnValidationConfig config = make_plant_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_FALSE(config.allow_hill);
  EXPECT_FALSE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_TRUE(config.check_roads);
  EXPECT_TRUE(config.check_slope);
  EXPECT_TRUE(config.check_river_margin);
}

TEST_F(SpawnValidatorTest, MakeStoneSpawnConfigDefaults) {
  SpawnValidationConfig config = make_stone_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_FALSE(config.allow_hill);
  EXPECT_FALSE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_FALSE(config.check_roads);
}

TEST_F(SpawnValidatorTest, MakeFirecampSpawnConfigDefaults) {
  SpawnValidationConfig config = make_firecamp_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_TRUE(config.allow_hill);
  EXPECT_FALSE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_TRUE(config.check_roads);
}

TEST_F(SpawnValidatorTest, MakeGrassSpawnConfigDefaults) {
  SpawnValidationConfig config = make_grass_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_FALSE(config.allow_hill);
  EXPECT_FALSE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_TRUE(config.check_roads);
}

TEST_F(SpawnValidatorTest, PlantSpawnConfigBlocksHills) {

  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::Hill;
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;

  SpawnValidator validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));

  EXPECT_TRUE(validator.can_spawn_at_grid(0.0F, 0.0F));
}

TEST_F(SpawnValidatorTest, TreeSpawnConfigRespectsRiverMargin) {

  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::River;
  build_cache();

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;

  SpawnValidator validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));

  EXPECT_FALSE(validator.can_spawn_at_grid(4.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(6.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 4.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 6.0F));

  EXPECT_TRUE(validator.can_spawn_at_grid(0.0F, 0.0F));
}
