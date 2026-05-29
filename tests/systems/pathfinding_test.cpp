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
  pathfinding.update_navigation_grid();

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
  pathfinding.update_navigation_grid();

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
  pathfinding.update_navigation_grid();

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
  pathfinding.update_navigation_grid();
  EXPECT_TRUE(pathfinding.is_tree(3, 4));

  std::uint64_t const tree_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(tree_id));
  ASSERT_TRUE(terrain.harvest_world_prop(tree_id));

  pathfinding.mark_region_dirty(2, 4, 3, 5);
  pathfinding.update_navigation_grid();

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
  pathfinding.update_navigation_grid();
  EXPECT_TRUE(pathfinding.is_boulder(3, 4));

  std::uint64_t const boulder_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(boulder_id));
  ASSERT_TRUE(terrain.harvest_world_prop(boulder_id));

  pathfinding.mark_region_dirty(2, 4, 3, 5);
  pathfinding.update_navigation_grid();

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
  pathfinding.update_navigation_grid();
  EXPECT_TRUE(pathfinding.is_iron_ore(3, 4));

  std::uint64_t const iron_ore_id = terrain.world_props().front().id;
  ASSERT_TRUE(terrain.reserve_world_prop(iron_ore_id));
  ASSERT_TRUE(terrain.harvest_world_prop(iron_ore_id));

  pathfinding.mark_region_dirty(2, 4, 3, 5);
  pathfinding.update_navigation_grid();

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
  pathfinding.update_navigation_grid();

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);
  pathfinding.update_navigation_grid();

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
      const auto* height_map = terrain.get_height_map();
      if (height_map != nullptr &&
          (height_map->isBridgeCell(x, z) || height_map->isBridgeCenterline(x, z) ||
           height_map->isHillEntrance(x, z))) {
        EXPECT_EQ(pathfinding.cell_value(x, z),
                  Game::Systems::Pathfinding::CellValue::Walkable)
            << "mandatory traversal cell at grid " << x << "," << z;
      } else {
        EXPECT_EQ(pathfinding.cell_value(x, z), *expected)
            << "resource marker at grid " << x << "," << z;
      }
      ++checked;
    }
  }

  EXPECT_GT(checked, 0);
}

TEST_F(PathfindingTest, BridgeDeckIsWalkableAndCrossesRiver) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.coordSystem = Game::Map::CoordSystem::World;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 2.0F});
  map_def.bridges.push_back(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 3.0F, 0.6F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map_def.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map_def.grid.height) * 0.5F - 0.5F));
  pathfinding.update_navigation_grid();

  auto const to_grid = [&map_def](float world_x, float world_z) {
    float const half_w = static_cast<float>(map_def.grid.width) * 0.5F - 0.5F;
    float const half_h = static_cast<float>(map_def.grid.height) * 0.5F - 0.5F;
    return Game::Systems::Point{
        static_cast<int>(std::round(world_x / map_def.grid.tile_size + half_w)),
        static_cast<int>(std::round(world_z / map_def.grid.tile_size + half_h))};
  };
  auto const center = to_grid(0.0F, 0.0F);
  auto const edge = to_grid(0.0F, 1.0F);

  EXPECT_TRUE(pathfinding.is_walkable(center.x, center.y));
  EXPECT_TRUE(pathfinding.is_walkable(edge.x, edge.y));

  auto const path = pathfinding.find_path({2, center.y}, {18, center.y});
  ASSERT_FALSE(path.empty());

  bool used_bridge_centerline = false;
  for (const auto& point : path) {
    if (point.x == center.x && point.y == center.y) {
      used_bridge_centerline = true;
    }
  }
  EXPECT_TRUE(used_bridge_centerline);
}

TEST_F(PathfindingTest, BridgeDeckRemainsWalkableWhenResourceMarkerOverlapsIt) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 2.0F});
  map_def.bridges.push_back(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 3.0F, 0.6F});
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = 0.0F, .z = 0.0F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map_def.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map_def.grid.height) * 0.5F - 0.5F));
  pathfinding.update_navigation_grid();

  auto const center = Game::Systems::Point{10, 10};
  ASSERT_TRUE(Game::Map::TerrainService::instance().get_height_map()->isBridgeCell(
      center.x, center.y));
  EXPECT_EQ(pathfinding.cell_value(center.x, center.y),
            Game::Systems::Pathfinding::CellValue::Walkable);
  EXPECT_TRUE(pathfinding.is_walkable(center.x, center.y));
  EXPECT_FALSE(pathfinding.find_path({2, center.y}, {18, center.y}).empty());
}

TEST_F(PathfindingTest, HillEntranceRemainsWalkableWhenResourceMarkerOverlapsIt) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.coordSystem = Game::Map::CoordSystem::World;
  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 10.0F;
  hill.depth = 10.0F;
  hill.height = 3.0F;
  hill.entrances.emplace_back(-5.0F, 0.0F, 0.0F);
  map_def.terrain.push_back(hill);
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::PineTree, .x = -5.0F, .z = 0.0F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map_def.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map_def.grid.height) * 0.5F - 0.5F));
  pathfinding.update_navigation_grid();

  auto const entrance = Game::Systems::Point{5, 10};
  auto const hilltop = Game::Systems::Point{10, 10};
  ASSERT_TRUE(Game::Map::TerrainService::instance().get_height_map()->isHillEntrance(
      entrance.x, entrance.y));
  EXPECT_EQ(pathfinding.cell_value(entrance.x, entrance.y),
            Game::Systems::Pathfinding::CellValue::Walkable);
  EXPECT_TRUE(pathfinding.is_walkable(entrance.x, entrance.y));
  EXPECT_TRUE(pathfinding.is_walkable_with_radius(entrance.x, entrance.y, 0.7F));
  EXPECT_FALSE(pathfinding.find_path({2, entrance.y}, hilltop, 0.7F).empty());
}

TEST_F(PathfindingTest, DiagonalBridgeCenterlineCanCrossRiver) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 4.0F});
  map_def.bridges.push_back(
      {QVector3D(-5.0F, 0.0F, -5.0F), QVector3D(5.0F, 0.0F, 5.0F), 2.0F, 0.6F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map_def.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map_def.grid.height) * 0.5F - 0.5F));
  pathfinding.update_navigation_grid();

  auto const to_grid = [&map_def](float world_x, float world_z) {
    float const half_w = static_cast<float>(map_def.grid.width) * 0.5F - 0.5F;
    float const half_h = static_cast<float>(map_def.grid.height) * 0.5F - 0.5F;
    return Game::Systems::Point{
        static_cast<int>(std::round(world_x / map_def.grid.tile_size + half_w)),
        static_cast<int>(std::round(world_z / map_def.grid.tile_size + half_h))};
  };

  auto const path =
      pathfinding.find_path(to_grid(-8.0F, -8.0F), to_grid(8.0F, 8.0F), 0.6F);
  ASSERT_FALSE(path.empty());

  bool used_bridge = false;
  for (const auto& point : path) {
    if (Game::Map::TerrainService::instance().get_height_map()->isBridgeCell(point.x,
                                                                             point.y)) {
      used_bridge = true;
      break;
    }
  }
  EXPECT_TRUE(used_bridge);
}

TEST_F(PathfindingTest, CrossingRhoneBridgeApproachRoutesFromBattleLogPosition) {
  Game::Map::MapDefinition map_def;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_crossing_rhone.json"), map_def, &error))
      << error.toStdString();

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map_def.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map_def.grid.height) * 0.5F - 0.5F));
  pathfinding.update_navigation_grid();

  auto const to_grid = [&map_def](float world_x, float world_z) {
    float const half_w = static_cast<float>(map_def.grid.width) * 0.5F - 0.5F;
    float const half_h = static_cast<float>(map_def.grid.height) * 0.5F - 0.5F;
    return Game::Systems::Point{
        static_cast<int>(std::round(world_x / map_def.grid.tile_size + half_w)),
        static_cast<int>(std::round(world_z / map_def.grid.tile_size + half_h))};
  };

  auto const start = to_grid(-53.5F, 33.9774F);
  auto const end = to_grid(37.5F, 56.5F);
  EXPECT_TRUE(pathfinding.is_walkable(start.x, start.y));
  EXPECT_TRUE(pathfinding.is_walkable_with_radius(start.x, start.y, 0.7F));
  EXPECT_TRUE(pathfinding.is_walkable(end.x, end.y));
  EXPECT_TRUE(pathfinding.is_walkable_with_radius(end.x, end.y, 0.7F));

  auto const path = pathfinding.find_path(start, end, 0.7F);
  EXPECT_FALSE(pathfinding.find_path(start, to_grid(-6.5F, 43.5F), 0.7F).empty());
  EXPECT_FALSE(pathfinding.find_path(to_grid(-6.5F, 43.5F), to_grid(-1.5F, 43.5F), 0.7F)
                   .empty());
  EXPECT_FALSE(pathfinding.find_path(to_grid(-1.5F, 43.5F), end, 0.7F).empty());

  ASSERT_FALSE(path.empty());

  bool used_bridge = false;
  for (const auto& point : path) {
    if (Game::Map::TerrainService::instance().get_height_map()->isBridgeCell(point.x,
                                                                             point.y)) {
      used_bridge = true;
      break;
    }
  }
  EXPECT_TRUE(used_bridge);
}

TEST_F(PathfindingTest, FindPathResolvesBlockedDestinationToNearestWalkableCell) {
  Game::Systems::Pathfinding pathfinding(8, 8);
  pathfinding.update_navigation_grid();
  pathfinding.set_obstacle(5, 5, true);

  auto const path = pathfinding.find_path({2, 2}, {5, 5});

  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path.front(), (Game::Systems::Point{2, 2}));
  EXPECT_NE(path.back(), (Game::Systems::Point{5, 5}));
  EXPECT_TRUE(pathfinding.is_walkable(path.back().x, path.back().y));
  EXPECT_LE(std::abs(path.back().x - 5), 1);
  EXPECT_LE(std::abs(path.back().y - 5), 1);
}

TEST_F(PathfindingTest, FindPathResolvesOutOfBoundsDestinationToNearestWalkableCell) {
  Game::Systems::Pathfinding pathfinding(8, 8);
  pathfinding.update_navigation_grid();

  auto const path = pathfinding.find_path({2, 2}, {32, 32});

  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path.front(), (Game::Systems::Point{2, 2}));
  EXPECT_EQ(path.back(), (Game::Systems::Point{7, 7}));
}

} // namespace
