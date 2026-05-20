#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
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

auto prop_grid_position(const Game::Map::MapDefinition& map_def,
                        const Game::Map::WorldProp& prop) -> Game::Systems::Point {
  if (map_def.coordSystem == Game::Map::CoordSystem::Grid) {
    return {static_cast<int>(std::round(prop.x)), static_cast<int>(std::round(prop.z))};
  }

  float const safe_tile_size = std::max(map_def.grid.tile_size, 0.0001F);
  float const half_w = static_cast<float>(map_def.grid.width) * 0.5F - 0.5F;
  float const half_h = static_cast<float>(map_def.grid.height) * 0.5F - 0.5F;
  return {static_cast<int>(std::round(prop.x / safe_tile_size + half_w)),
          static_cast<int>(std::round(prop.z / safe_tile_size + half_h))};
}

auto cell_value_for_prop(const Game::Map::WorldProp& prop)
    -> std::optional<Game::Systems::Pathfinding::CellValue> {
  if (Game::Map::is_tree_world_prop_type(prop.type)) {
    return Game::Systems::Pathfinding::CellValue::Tree;
  }
  if (Game::Map::is_boulder_world_prop_type(prop.type)) {
    return Game::Systems::Pathfinding::CellValue::Boulder;
  }
  if (Game::Map::is_iron_ore_world_prop_type(prop.type)) {
    return Game::Systems::Pathfinding::CellValue::IronOre;
  }
  return std::nullopt;
}

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

TEST_F(PathfindingTest, RuntimeHarvestPropsAreMarkedAfterTerrainLoads) {
  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_forest.json"), map_def, &error))
      << error.toStdString();

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map_def.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map_def.grid.height) * 0.5F - 0.5F));
  pathfinding.update_building_obstacles();

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  pathfinding.update_building_obstacles();

  using CellValue = Game::Systems::Pathfinding::CellValue;
  std::vector<std::optional<CellValue>> expected_cells(
      static_cast<std::size_t>(map_def.grid.width * map_def.grid.height));
  for (const auto& prop : terrain.world_props()) {
    const auto expected = cell_value_for_prop(prop);
    if (!expected.has_value()) {
      continue;
    }

    const auto grid = prop_grid_position(map_def, prop);
    if (grid.x < 0 || grid.x >= map_def.grid.width || grid.y < 0 ||
        grid.y >= map_def.grid.height) {
      continue;
    }
    expected_cells[static_cast<std::size_t>(grid.y * map_def.grid.width + grid.x)] =
        *expected;
  }

  int checked = 0;
  for (int z = 0; z < map_def.grid.height; ++z) {
    for (int x = 0; x < map_def.grid.width; ++x) {
      const auto& expected =
          expected_cells[static_cast<std::size_t>(z * map_def.grid.width + x)];
      if (!expected.has_value()) {
        continue;
      }
      EXPECT_EQ(pathfinding.cell_value(x, z), *expected)
          << "resource marker at grid " << x << "," << z;
      ++checked;
    }
  }

  EXPECT_GT(checked, 0);
}

} // namespace
