#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/pathfinding.h"

namespace {

class PathfindingTest : public ::testing::Test {
protected:
  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }
};

TEST_F(PathfindingTest, TreeCellsRemainBlockedButDistinguishable) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 32;
  map_def.grid.height = 32;
  map_def.grid.tile_size = 1.0F;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = 3.0F, .z = 4.0F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(32, 32);
  pathfinding.set_grid_offset(-(32.0F * 0.5F - 0.5F), -(32.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();

  EXPECT_EQ(pathfinding.cell_value(3, 4), Game::Systems::Pathfinding::CellValue::Tree);
  EXPECT_TRUE(pathfinding.is_tree(3, 4));
  EXPECT_FALSE(pathfinding.is_walkable(3, 4));
}

TEST_F(PathfindingTest, BoulderCellsRemainBlockedButDistinguishable) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 32;
  map_def.grid.height = 32;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::Boulder, .x = 3.0F, .z = 4.0F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(32, 32);
  pathfinding.set_grid_offset(-(32.0F * 0.5F - 0.5F), -(32.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();

  EXPECT_EQ(pathfinding.cell_value(3, 4),
            Game::Systems::Pathfinding::CellValue::Boulder);
  EXPECT_TRUE(pathfinding.is_boulder(3, 4));
  EXPECT_FALSE(pathfinding.is_walkable(3, 4));
}

TEST_F(PathfindingTest, IronOreCellsRemainBlockedButDistinguishable) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 16;
  map_def.grid.height = 16;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::IronOre, .x = 3.0F, .z = 4.0F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(16, 16);
  pathfinding.set_grid_offset(-(16.0F * 0.5F - 0.5F), -(16.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();

  EXPECT_EQ(pathfinding.cell_value(3, 4),
            Game::Systems::Pathfinding::CellValue::IronOre);
  EXPECT_TRUE(pathfinding.is_iron_ore(3, 4));
  EXPECT_FALSE(pathfinding.is_walkable(3, 4));
}

TEST_F(PathfindingTest, HarvestedTreeClearsTreeMarkerAfterDirtyUpdate) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 16;
  map_def.grid.height = 16;
  map_def.grid.tile_size = 1.0F;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = 3.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  Game::Systems::Pathfinding pathfinding(16, 16);
  pathfinding.set_grid_offset(-(16.0F * 0.5F - 0.5F), -(16.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();
  EXPECT_TRUE(pathfinding.is_tree(3, 4));

  std::uint64_t const tree_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(tree_id));
  ASSERT_TRUE(terrain.harvest_world_prop(tree_id));

  pathfinding.mark_region_dirty(2, 4, 3, 5);
  pathfinding.update_building_obstacles();

  EXPECT_FALSE(pathfinding.is_tree(3, 4));
  EXPECT_TRUE(pathfinding.is_walkable(3, 4));
}

TEST_F(PathfindingTest, HarvestedBoulderClearsMarkerAfterDirtyUpdate) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 16;
  map_def.grid.height = 16;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::Boulder, .x = 3.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  Game::Systems::Pathfinding pathfinding(16, 16);
  pathfinding.set_grid_offset(-(16.0F * 0.5F - 0.5F), -(16.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();
  EXPECT_TRUE(pathfinding.is_boulder(3, 4));

  std::uint64_t const boulder_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(boulder_id));
  ASSERT_TRUE(terrain.harvest_world_prop(boulder_id));

  pathfinding.mark_region_dirty(2, 4, 3, 5);
  pathfinding.update_building_obstacles();

  EXPECT_FALSE(pathfinding.is_boulder(3, 4));
  EXPECT_TRUE(pathfinding.is_walkable(3, 4));
}

TEST_F(PathfindingTest, HarvestedIronOreClearsMarkerAfterDirtyUpdate) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 16;
  map_def.grid.height = 16;
  map_def.grid.tile_size = 1.0F;
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::IronOre, .x = 3.0F, .z = 4.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  ASSERT_EQ(terrain.world_props().size(), 1U);

  Game::Systems::Pathfinding pathfinding(16, 16);
  pathfinding.set_grid_offset(-(16.0F * 0.5F - 0.5F), -(16.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();
  EXPECT_TRUE(pathfinding.is_iron_ore(3, 4));

  std::uint64_t const iron_ore_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(iron_ore_id));
  ASSERT_TRUE(terrain.harvest_world_prop(iron_ore_id));

  pathfinding.mark_region_dirty(2, 4, 3, 5);
  pathfinding.update_building_obstacles();

  EXPECT_FALSE(pathfinding.is_iron_ore(3, 4));
  EXPECT_TRUE(pathfinding.is_walkable(3, 4));
}

TEST_F(PathfindingTest, ProceduralIronOreMarksPathfindingGrid) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 96;
  map_def.grid.height = 96;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.seed = 42U;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  auto first_iron_ore_it =
      std::find_if(terrain.world_props().begin(),
                   terrain.world_props().end(),
                   [](const Game::Map::WorldProp& prop) {
                     return Game::Map::is_iron_ore_world_prop_type(prop.type);
                   });
  ASSERT_NE(first_iron_ore_it, terrain.world_props().end());

  Game::Systems::Pathfinding pathfinding(96, 96);
  pathfinding.set_grid_offset(-(96.0F * 0.5F - 0.5F), -(96.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();

  int const iron_ore_x = static_cast<int>(std::round(first_iron_ore_it->x));
  int const iron_ore_z = static_cast<int>(std::round(first_iron_ore_it->z));
  EXPECT_TRUE(pathfinding.is_iron_ore(iron_ore_x, iron_ore_z));
}

TEST_F(PathfindingTest, ProceduralTreesMarkPathfindingGrid) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 64;
  map_def.grid.height = 64;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.seed = 42U;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::ForestMud);

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  auto first_tree_it =
      std::find_if(terrain.world_props().begin(),
                   terrain.world_props().end(),
                   [](const Game::Map::WorldProp& prop) {
                     return Game::Map::is_tree_world_prop_type(prop.type);
                   });
  ASSERT_NE(first_tree_it, terrain.world_props().end());

  Game::Systems::Pathfinding pathfinding(64, 64);
  pathfinding.set_grid_offset(-(64.0F * 0.5F - 0.5F), -(64.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();

  int const tree_x = static_cast<int>(std::round(first_tree_it->x));
  int const tree_z = static_cast<int>(std::round(first_tree_it->z));
  EXPECT_TRUE(pathfinding.is_tree(tree_x, tree_z));
}

TEST_F(PathfindingTest, ProceduralBouldersMarkPathfindingGrid) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 64;
  map_def.grid.height = 64;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.seed = 42U;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  auto first_boulder_it =
      std::find_if(terrain.world_props().begin(),
                   terrain.world_props().end(),
                   [](const Game::Map::WorldProp& prop) {
                     return Game::Map::is_boulder_world_prop_type(prop.type);
                   });
  ASSERT_NE(first_boulder_it, terrain.world_props().end());

  Game::Systems::Pathfinding pathfinding(64, 64);
  pathfinding.set_grid_offset(-(64.0F * 0.5F - 0.5F), -(64.0F * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();

  int const boulder_x = static_cast<int>(std::round(first_boulder_it->x));
  int const boulder_z = static_cast<int>(std::round(first_boulder_it->z));
  EXPECT_TRUE(pathfinding.is_boulder(boulder_x, boulder_z));
}

} // namespace
