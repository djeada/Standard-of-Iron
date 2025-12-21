#include <gtest/gtest.h>

#include "render/ground/spawn_validator.h"

using namespace Render::Ground;
using namespace Game::Map;

class SpawnValidatorTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a simple 10x10 terrain for testing
    width = 10;
    height = 10;
    tile_size = 1.0F;

    // Initialize height data (flat terrain)
    height_data.resize(static_cast<size_t>(width * height), 0.0F);

    // Initialize terrain types (all flat by default)
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

  // Flat terrain should return 0 everywhere
  EXPECT_FLOAT_EQ(terrain_cache.sample_height_at(5.0F, 5.0F), 0.0F);
  EXPECT_FLOAT_EQ(terrain_cache.sample_height_at(0.0F, 0.0F), 0.0F);
  EXPECT_FLOAT_EQ(terrain_cache.sample_height_at(9.0F, 9.0F), 0.0F);
}

TEST_F(SpawnValidatorTest, TerrainCacheGetSlopeFlat) {
  build_cache();

  // Flat terrain should have zero slope
  float slope = terrain_cache.get_slope_at(5, 5);
  EXPECT_LT(slope, 0.01F);
}

TEST_F(SpawnValidatorTest, TerrainCacheGetTerrainType) {
  // Set some specific terrain types
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
  config.edge_padding = 0.0F; // No edge padding for this test

  SpawnValidator validator(terrain_cache, config);

  // Center of map should be valid for spawning on flat terrain
  EXPECT_TRUE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksMountainTerrain) {
  // Make center a mountain
  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::Mountain;
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.allow_mountain = false;

  SpawnValidator validator(terrain_cache, config);

  // Mountain should not be valid for spawning
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksRiverTerrain) {
  // Make center a river
  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::River;
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.allow_river = false;

  SpawnValidator validator(terrain_cache, config);

  // River should not be valid for spawning
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorRiverMarginCheck) {
  // Make a cell a river
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

  // Adjacent cell should also be blocked due to river margin
  EXPECT_FALSE(validator.can_spawn_at_grid(4.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(6.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 4.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 6.0F));

  // Cell far from river should be valid
  EXPECT_TRUE(validator.can_spawn_at_grid(0.0F, 0.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorEdgePaddingCheck) {
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.2F; // 20% edge padding

  SpawnValidator validator(terrain_cache, config);

  // Edge positions should be blocked
  EXPECT_FALSE(validator.can_spawn_at_grid(0.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 0.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(9.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 9.0F));

  // Center should be valid
  EXPECT_TRUE(validator.can_spawn_at_grid(5.0F, 5.0F));
}

TEST_F(SpawnValidatorTest, GridToWorldConversion) {
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = 2.0F; // 2 units per tile
  config.edge_padding = 0.0F;

  SpawnValidator validator(terrain_cache, config);

  float world_x = 0.0F;
  float world_z = 0.0F;

  // Center of 10x10 grid should be at (0, 0) in world coordinates
  // half_width = 10 * 0.5 - 0.5 = 4.5
  // world_x = (5.0 - 4.5) * 2.0 = 1.0
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

TEST_F(SpawnValidatorTest, MakeTreeSpawnConfigDefaults) {
  SpawnValidationConfig config = make_tree_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_TRUE(config.allow_hill);
  EXPECT_TRUE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_TRUE(config.check_roads);
  EXPECT_TRUE(config.check_river_margin);
  EXPECT_EQ(config.river_margin, 1);
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
  // Make center a hill
  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::Hill;
  build_cache();

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;

  SpawnValidator validator(terrain_cache, config);

  // Hill should not be valid for plant spawning
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));
  
  // But flat terrain should be valid
  EXPECT_TRUE(validator.can_spawn_at_grid(0.0F, 0.0F));
}

TEST_F(SpawnValidatorTest, TreeSpawnConfigRespectsRiverMargin) {
  // Make center a river
  terrain_types[static_cast<size_t>(5 * width + 5)] = TerrainType::River;
  build_cache();

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;

  SpawnValidator validator(terrain_cache, config);

  // River should not be valid for tree spawning
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));
  
  // Adjacent cells should also be blocked due to river margin
  EXPECT_FALSE(validator.can_spawn_at_grid(4.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(6.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 4.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 6.0F));

  // Cell far from river should be valid
  EXPECT_TRUE(validator.can_spawn_at_grid(0.0F, 0.0F));
}
