#include <gtest/gtest.h>

#include "game/map/map_definition.h"
#include "game/map/scatter/spawn_validator.h"
#include "game/map/scatter/world_prop_clearance_index.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"

using namespace Render::Ground;
using namespace Game::Map;

class SpawnValidatorTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();

    width = 10;
    height = 10;
    tile_size = 1.0F;

    height_data.resize(static_cast<size_t>(width * height), 0.0F);

    terrain_types.resize(static_cast<size_t>(width * height), TerrainType::Flat);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  void build_cache() {
    terrain_cache.build_from_height_map(
        height_data, terrain_types, width, height, tile_size);
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

  float const slope = terrain_cache.get_slope_at(5, 5);
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

  SpawnValidator const validator(terrain_cache, config);

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

  SpawnValidator const validator(terrain_cache, config);

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

  SpawnValidator const validator(terrain_cache, config);

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

  SpawnValidator const validator(terrain_cache, config);

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

  SpawnValidator const validator(terrain_cache, config);

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

  SpawnValidator const validator(terrain_cache, config);

  float world_x = 0.0F;
  float world_z = 0.0F;

  validator.grid_to_world(5.0F, 5.0F, world_x, world_z);
  EXPECT_NEAR(world_x, 1.0F, 0.01F);
  EXPECT_NEAR(world_z, 1.0F, 0.01F);
}

TEST_F(SpawnValidatorTest, MakePlantSpawnConfigDefaults) {
  SpawnValidationConfig const config = make_plant_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_FALSE(config.allow_hill);
  EXPECT_FALSE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_TRUE(config.check_roads);
  EXPECT_TRUE(config.check_slope);
  EXPECT_TRUE(config.check_river_margin);
  EXPECT_GT(config.building_clearance, 0.0F);
}

TEST_F(SpawnValidatorTest, MakeStoneSpawnConfigDefaults) {
  SpawnValidationConfig const config = make_stone_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_FALSE(config.allow_hill);
  EXPECT_FALSE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_TRUE(config.check_roads);
  EXPECT_GT(config.road_clearance, 0.0F);
}

TEST_F(SpawnValidatorTest, MakeFirecampSpawnConfigDefaults) {
  SpawnValidationConfig const config = make_firecamp_spawn_config();

  EXPECT_TRUE(config.allow_flat);
  EXPECT_TRUE(config.allow_hill);
  EXPECT_FALSE(config.allow_mountain);
  EXPECT_FALSE(config.allow_river);
  EXPECT_TRUE(config.check_buildings);
  EXPECT_TRUE(config.check_roads);
}

TEST_F(SpawnValidatorTest, MakeGrassSpawnConfigDefaults) {
  SpawnValidationConfig const config = make_grass_spawn_config();

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

  SpawnValidator const validator(terrain_cache, config);

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

  SpawnValidator const validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 5.0F));

  EXPECT_FALSE(validator.can_spawn_at_grid(4.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(6.0F, 5.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 4.0F));
  EXPECT_FALSE(validator.can_spawn_at_grid(5.0F, 6.0F));

  EXPECT_TRUE(validator.can_spawn_at_grid(0.0F, 0.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksBuildingClearance) {
  build_cache();

  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.register_building(1U, "barracks", 0.0F, 0.0F, 0);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.building_clearance = 2.0F;
  config.check_roads = false;
  config.check_bridges = false;
  config.check_river_margin = false;
  config.river_clearance = 0.0F;

  SpawnValidator const validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_world(0.0F, 0.0F));
  EXPECT_FALSE(validator.can_spawn_at_world(3.5F, 0.0F));
  EXPECT_TRUE(validator.can_spawn_at_world(4.2F, 0.0F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksRoadClearance) {
  build_cache();

  Game::Map::MapDefinition map_def;
  map_def.grid.width = width;
  map_def.grid.height = height;
  map_def.grid.tile_size = tile_size;
  map_def.roads.push_back(
      {QVector3D(-4.0F, 0.0F, 0.0F), QVector3D(4.0F, 0.0F, 0.0F), 2.0F});
  Game::Map::TerrainService::instance().initialize(map_def);

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.check_bridges = false;
  config.check_river_margin = false;
  config.river_clearance = 0.0F;

  SpawnValidator const validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_world(0.0F, 1.2F));
  EXPECT_TRUE(validator.can_spawn_at_world(0.0F, 1.8F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksBridgeClearance) {

  width = 24;
  height = 24;
  height_data.assign(static_cast<std::size_t>(width * height), 0.0F);
  terrain_types.assign(static_cast<std::size_t>(width * height), TerrainType::Flat);
  build_cache();

  Game::Map::MapDefinition map_def;
  map_def.grid.width = width;
  map_def.grid.height = height;
  map_def.grid.tile_size = tile_size;
  map_def.bridges.push_back({QVector3D(-3.0F, 0.0F, 0.0F),
                             QVector3D(3.0F, 0.0F, 0.0F),
                             Game::Map::k_min_bridge_width,
                             0.5F});
  Game::Map::TerrainService::instance().initialize(map_def);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.check_roads = false;
  config.check_river_margin = false;
  config.river_clearance = 0.0F;

  SpawnValidator const validator(terrain_cache, config);
  float const exclusion_radius =
      Game::Map::k_min_bridge_width * 0.5F + config.bridge_clearance;

  EXPECT_FALSE(validator.can_spawn_at_world(0.0F, exclusion_radius - 0.1F));
  EXPECT_TRUE(validator.can_spawn_at_world(0.0F, exclusion_radius + 0.1F));
}

TEST_F(SpawnValidatorTest, SpawnValidatorBlocksRiverClearance) {
  build_cache();

  Game::Map::MapDefinition map_def;
  map_def.grid.width = width;
  map_def.grid.height = height;
  map_def.grid.tile_size = tile_size;
  map_def.rivers.push_back(
      {QVector3D(-4.0F, 0.0F, 0.0F), QVector3D(4.0F, 0.0F, 0.0F), 2.0F});
  Game::Map::TerrainService::instance().initialize(map_def);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.check_roads = false;
  config.check_bridges = false;
  config.check_river_margin = false;

  SpawnValidator const validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_world(0.0F, 2.2F));
  EXPECT_TRUE(validator.can_spawn_at_world(0.0F, 2.8F));
}

TEST(WorldPropClearanceIndexTest, ASolidPropClaimsItsGroundBody) {
  WorldPropClearanceIndex index;

  WorldProp tent;
  tent.type = WorldProp::Type::Tent;
  tent.x = 12.0F;
  tent.z = -7.0F;
  tent.scale = 1.0F;
  index.rebuild({tent}, 4.0F);

  const auto body = world_prop_ground_half_extents(WorldProp::Type::Tent, 1.0F);
  ASSERT_GT(body.x, 0.0F);
  EXPECT_EQ(index.body_count(), 1U);

  EXPECT_TRUE(index.overlaps(tent.x, tent.z, 0.0F));
  EXPECT_TRUE(index.overlaps(tent.x + body.x * 0.5F, tent.z, 0.0F));
  EXPECT_FALSE(index.overlaps(tent.x + body.x + 0.5F, tent.z, 0.0F));

  EXPECT_TRUE(index.overlaps(tent.x + body.x + 0.3F, tent.z, 0.5F))
      << "a scatter item's own footprint has to count, or a stone lands with "
         "half of itself inside the tent";

  ASSERT_GT(body.z, body.x);
  EXPECT_TRUE(index.overlaps(tent.x, tent.z + body.x + 0.2F, 0.0F))
      << "ground under the awning is being handed back to scatter";
  EXPECT_FALSE(index.overlaps(tent.x + body.x + 0.2F, tent.z + body.z + 0.2F, 0.0F))
      << "the corner outside both axes is open ground";
}

TEST(WorldPropClearanceIndexTest, ATreeOnlyClaimsItsStem) {
  WorldPropClearanceIndex index;

  WorldProp pine;
  pine.type = WorldProp::Type::PineTree;
  pine.x = 0.0F;
  pine.z = 0.0F;
  pine.scale = 1.0F;
  index.rebuild({pine}, 4.0F);

  const float stem = world_prop_ground_radius(WorldProp::Type::PineTree, 1.0F);
  const float canopy = world_prop_render_scale(WorldProp::Type::PineTree) *
                       world_prop_model_half_extent(WorldProp::Type::PineTree);
  ASSERT_LT(stem, canopy);

  EXPECT_TRUE(index.overlaps(stem * 0.5F, 0.0F, 0.0F));
  EXPECT_FALSE(index.overlaps(canopy * 0.9F, 0.0F, 0.0F))
      << "grass and stones belong under a canopy; only the trunk is solid";
}

TEST(WorldPropClearanceIndexTest, PropsFurtherApartThanOneBucketStillRegister) {
  WorldPropClearanceIndex index;

  std::vector<WorldProp> props;
  for (int i = 0; i < 40; ++i) {
    WorldProp ruin;
    ruin.type = WorldProp::Type::Ruins;
    ruin.x = static_cast<float>(i) * 37.0F;
    ruin.z = static_cast<float>(-i) * 21.0F;
    ruin.scale = 1.0F;
    props.push_back(ruin);
  }
  index.rebuild(props, 4.0F);

  for (const auto& ruin : props) {
    EXPECT_TRUE(index.overlaps(ruin.x, ruin.z, 0.0F))
        << "bucket lookup missed a prop at " << ruin.x << ", " << ruin.z;
  }
  EXPECT_FALSE(index.overlaps(5000.0F, 5000.0F, 0.0F));
}

TEST_F(SpawnValidatorTest, ScatterRefusesTheGroundAnAuthoredPropStandsOn) {

  width = 40;
  height = 40;
  height_data.assign(static_cast<std::size_t>(width * height), 0.0F);
  terrain_types.assign(static_cast<std::size_t>(width * height), TerrainType::Flat);
  build_cache();

  Game::Map::MapDefinition map_def;
  map_def.grid.width = width;
  map_def.grid.height = height;
  map_def.grid.tile_size = tile_size;

  WorldProp ruin;
  ruin.type = WorldProp::Type::Ruins;
  ruin.x = 28.0F;
  ruin.z = 12.0F;
  ruin.scale = 1.0F;
  map_def.world_props.push_back(ruin);
  Game::Map::TerrainService::instance().initialize(map_def);

  const QVector3D drawn =
      Game::Map::TerrainService::instance().world_prop_world_position(ruin);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = 0.0F;
  config.check_roads = false;
  config.check_bridges = false;
  config.check_river_margin = false;
  config.river_clearance = 0.0F;
  config.building_clearance = 0.0F;
  config.check_buildings = false;

  SpawnValidator const validator(terrain_cache, config);

  EXPECT_FALSE(validator.can_spawn_at_world(drawn.x(), drawn.z()))
      << "a stone is being scattered onto the middle of a ruin: the clearance "
         "index and the scatter query are not in the same coordinate space";
}

TEST(WorldPropClearanceIndexTest, TheIndexAnswersInTheSpaceScatterAsksIn) {

  Game::Map::TerrainService::instance().clear();

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 40;
  map_def.grid.height = 40;
  map_def.grid.tile_size = 1.0F;

  WorldProp tent;
  tent.type = WorldProp::Type::Tent;
  tent.x = 31.0F;
  tent.z = 9.0F;
  tent.scale = 1.0F;
  map_def.world_props.push_back(tent);
  Game::Map::TerrainService::instance().initialize(map_def);

  const QVector3D drawn =
      Game::Map::TerrainService::instance().world_prop_world_position(tent);

  const auto& index = shared_world_prop_clearance_index();
  ASSERT_FALSE(index.empty());

  EXPECT_TRUE(index.overlaps(drawn.x(), drawn.z(), 0.0F))
      << "the index does not claim the ground the tent is drawn on";
  EXPECT_FALSE(index.overlaps(tent.x, tent.z, 0.0F))
      << "the index is still holding authored grid coordinates as if they were "
         "world coordinates";

  Game::Map::TerrainService::instance().clear();
}
