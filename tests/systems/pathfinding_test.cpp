#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
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

TEST_F(PathfindingTest, TheNearestStandingCellFacesWhoeverIsWalkingIn) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 24;
  map_def.grid.height = 24;
  map_def.grid.tile_size = 1.0F;
  Game::Map::apply_ground_type_defaults(map_def.biome,
                                        Game::Map::GroundType::SoilRocky);
  map_def.world_props.push_back(
      {.type = Game::Map::WorldProp::Type::Boulder, .x = 12.0F, .z = 12.0F});

  Game::Map::TerrainService::instance().initialize(map_def);
  Game::Systems::NavGrid::initialize(map_def.grid.width, map_def.grid.height);

  const Game::Systems::Point node = prop_grid_position(
      map_def, Game::Map::TerrainService::instance().world_props().front());

  struct Side {
    const char* name;
    int dx;
    int dz;
  };
  const Side sides[] = {
      {"north", 0, -6}, {"south", 0, 6}, {"west", -6, 0}, {"east", 6, 0}};

  for (const auto& side : sides) {
    const QVector3D approach =
        Game::Systems::NavGrid::grid_to_world({node.x + side.dx, node.y + side.dz});
    const auto cell =
        Game::Systems::NavGrid::find_nearest_walkable_grid_facing(node, approach, 4);
    ASSERT_TRUE(cell.has_value()) << side.name;

    int const cell_dx = cell->x - node.x;
    int const cell_dz = cell->y - node.y;
    EXPECT_LE(std::abs(cell_dx), 1) << side.name;
    EXPECT_LE(std::abs(cell_dz), 1) << side.name;
    EXPECT_GT((cell_dx * side.dx) + (cell_dz * side.dz), 0)
        << side.name
        << ": the standing cell must sit between the worker and the rock, not on a "
           "fixed side of it";
  }
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

  QVector3D const beside_tree = pathfinding.grid_to_world({4, 4});
  EXPECT_TRUE(pathfinding.is_world_position_walkable(beside_tree));
  EXPECT_FALSE(pathfinding.is_world_position_walkable(
      beside_tree, Game::Systems::Pathfinding::Passability::Light, 0.6F));
}

TEST_F(PathfindingTest, FormationClearanceRoutesAroundBlockedFootprints) {
  Game::Systems::Pathfinding pathfinding(9, 9);
  pathfinding.set_grid_offset(-4.0F, -4.0F);
  pathfinding.update_navigation_grid();
  pathfinding.set_obstacle(4, 4, true);

  constexpr float k_clearance = 0.6F;
  auto const path = pathfinding.find_path(
      {1, 4}, {7, 4}, Game::Systems::Pathfinding::Passability::Light, k_clearance);

  ASSERT_FALSE(path.empty());
  for (auto const& cell : path) {
    EXPECT_TRUE(pathfinding.is_world_position_walkable(
        pathfinding.grid_to_world(cell),
        Game::Systems::Pathfinding::Passability::Light,
        k_clearance));
  }
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

  bool used_bridge_cell = false;
  for (const auto& point : path) {
    if (point.x == center.x && point.y == center.y) {
      used_bridge_cell = true;
    }
  }
  EXPECT_TRUE(used_bridge_cell);
}

TEST_F(PathfindingTest, TerrainClearRebuildsStaleTopologyCells) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 2.0F});

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-10.0F, -10.0F);
  pathfinding.update_navigation_grid();
  ASSERT_FALSE(pathfinding.is_walkable(10, 10));

  int east_bank = 11;
  while (east_bank < map_def.grid.width && !pathfinding.is_walkable(east_bank, 10)) {
    ++east_bank;
  }
  ASSERT_LT(east_bank, map_def.grid.width);
  QVector3D const bank_world = pathfinding.grid_to_world({east_bank, 10});
  EXPECT_TRUE(pathfinding.is_world_position_walkable(bank_world));
  EXPECT_FALSE(pathfinding.is_world_position_walkable(
      bank_world, Game::Systems::Pathfinding::Passability::Light, 0.6F));

  terrain.clear();
  pathfinding.update_navigation_grid();

  EXPECT_TRUE(pathfinding.is_walkable(10, 10));
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
  EXPECT_TRUE(pathfinding.is_walkable(entrance.x, entrance.y));
  EXPECT_FALSE(pathfinding.find_path({2, entrance.y}, hilltop).empty());
}

TEST_F(PathfindingTest, HillFlanksBlockTraversalAndRouteUnitsThroughEntrance) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 41;
  map_def.grid.height = 41;
  map_def.grid.tile_size = 1.0F;
  map_def.coordSystem = Game::Map::CoordSystem::World;
  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 20.0F;
  hill.depth = 20.0F;
  hill.height = 5.0F;
  hill.entrances.emplace_back(-10.0F, 0.0F, 0.0F);
  map_def.terrain.push_back(hill);

  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(map_def);

  Game::Systems::Pathfinding pathfinding(41, 41);
  pathfinding.set_grid_offset(-20.0F, -20.0F);
  pathfinding.update_navigation_grid();

  const Game::Systems::Point crown{20, 20};
  const Game::Systems::Point east_flank{29, 20};
  const Game::Systems::Point north_flank{20, 29};
  EXPECT_TRUE(pathfinding.is_walkable(crown.x, crown.y));
  EXPECT_FALSE(pathfinding.is_walkable(east_flank.x, east_flank.y));
  EXPECT_FALSE(pathfinding.is_walkable(north_flank.x, north_flank.y));
  EXPECT_FALSE(terrain.is_hill_entrance(east_flank.x, east_flank.y));
  EXPECT_FALSE(terrain.is_hill_entrance(north_flank.x, north_flank.y));

  const auto path = pathfinding.find_path({33, 20}, crown);
  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path.back().x, crown.x);
  EXPECT_EQ(path.back().y, crown.y);
  EXPECT_TRUE(std::any_of(path.begin(), path.end(), [&](const auto& point) {
    return terrain.is_hill_entrance(point.x, point.y);
  }));
}

TEST_F(PathfindingTest, DiagonalBridgeCellsCanCrossRiver) {
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

  auto const path = pathfinding.find_path(to_grid(-8.0F, -8.0F), to_grid(8.0F, 8.0F));
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

TEST_F(PathfindingTest, BridgeApproachSegmentIsWalkableThroughGridCells) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 21;
  map_def.grid.height = 21;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -10.0F), QVector3D(0.0F, 0.0F, 10.0F), 2.0F});
  map_def.bridges.push_back(
      {QVector3D(-2.0F, 0.0F, 0.0F), QVector3D(2.0F, 0.0F, 0.0F), 2.0F, 0.6F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(map_def.grid.width, map_def.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map_def.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map_def.grid.height) * 0.5F - 0.5F));
  pathfinding.update_navigation_grid();

  EXPECT_TRUE(pathfinding.is_world_segment_walkable(QVector3D(-4.0F, 0.0F, 0.0F),
                                                    QVector3D(-1.5F, 0.0F, 0.0F)));
}

TEST_F(PathfindingTest, CrossingRhoneAuthoredBridgeRoutesAcrossRiver) {
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

  ASSERT_FALSE(map_def.bridges.empty());
  auto const& bridge = map_def.bridges.front();
  auto const start = to_grid(bridge.start.x(), bridge.start.z());
  auto const end = to_grid(bridge.end.x(), bridge.end.z());
  EXPECT_TRUE(pathfinding.is_walkable(start.x, start.y));
  EXPECT_TRUE(pathfinding.is_walkable(start.x, start.y));
  EXPECT_TRUE(pathfinding.is_walkable(end.x, end.y));
  EXPECT_TRUE(pathfinding.is_walkable(end.x, end.y));

  auto const path = pathfinding.find_path(start, end);

  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path.front(), start);
  EXPECT_EQ(path.back(), end);

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

TEST_F(PathfindingTest, FindPathCanRecoverFromBlockedStartCell) {
  Game::Systems::Pathfinding pathfinding(8, 8);
  pathfinding.update_navigation_grid();
  pathfinding.set_obstacle(2, 2, true);

  auto const path = pathfinding.find_path({2, 2}, {6, 6});

  ASSERT_FALSE(path.empty());
  EXPECT_NE(path.front(), (Game::Systems::Point{2, 2}));
  EXPECT_TRUE(pathfinding.is_walkable(path.front().x, path.front().y));
  EXPECT_EQ(path.back(), (Game::Systems::Point{6, 6}));
}

TEST_F(PathfindingTest, FindPathResolvesOutOfBoundsDestinationToNearestWalkableCell) {
  Game::Systems::Pathfinding pathfinding(8, 8);
  pathfinding.update_navigation_grid();

  auto const path = pathfinding.find_path({2, 2}, {32, 32});

  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path.front(), (Game::Systems::Point{2, 2}));
  EXPECT_EQ(path.back(), (Game::Systems::Point{7, 7}));
}

TEST_F(PathfindingTest, FindPathReturnsClosestReachableRouteWhenGoalIsSealedOff) {
  Game::Systems::Pathfinding pathfinding(8, 8);
  pathfinding.update_navigation_grid();
  for (int y = 0; y < 8; ++y) {
    pathfinding.set_obstacle(4, y, true);
  }

  auto const path = pathfinding.find_path({1, 4}, {7, 4});

  ASSERT_FALSE(path.empty());
  EXPECT_EQ(path.front(), (Game::Systems::Point{1, 4}));
  EXPECT_NE(path.back(), (Game::Systems::Point{7, 4}));
  EXPECT_LT(path.back().x, 4);
  EXPECT_TRUE(pathfinding.is_walkable(path.back().x, path.back().y));
}

TEST_F(PathfindingTest, ForestGroundOnlyLightUnitsMayEnter) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 32;
  map_def.grid.height = 32;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.procedural_trees_enabled = false;
  map_def.forests.push_back({.id = "wood", .x = 16.0F, .z = 16.0F, .radius = 4.0F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(32, 32);
  pathfinding.update_navigation_grid();

  using Passability = Game::Systems::Pathfinding::Passability;

  EXPECT_EQ(pathfinding.cell_value(16, 16),
            Game::Systems::Pathfinding::CellValue::Forest);
  EXPECT_TRUE(pathfinding.is_forest(16, 16));
  EXPECT_TRUE(pathfinding.is_walkable(16, 16, Passability::Light));
  EXPECT_FALSE(pathfinding.is_walkable(16, 16, Passability::Heavy));

  EXPECT_FALSE(pathfinding.is_forest(2, 2)) << "ground outside the radius stays open";
  EXPECT_TRUE(pathfinding.is_walkable(2, 2, Passability::Heavy));
}

TEST_F(PathfindingTest, HeavyUnitsRouteAroundAWoodThatLightUnitsWalkThrough) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 41;
  map_def.grid.height = 41;
  map_def.grid.tile_size = 1.0F;
  map_def.biome.procedural_trees_enabled = false;
  map_def.forests.push_back({.id = "screen", .x = 20.0F, .z = 20.0F, .radius = 6.0F});

  Game::Map::TerrainService::instance().initialize(map_def);

  Game::Systems::Pathfinding pathfinding(41, 41);
  pathfinding.update_navigation_grid();

  using Passability = Game::Systems::Pathfinding::Passability;
  const Game::Systems::Point start{20, 8};
  const Game::Systems::Point goal{20, 32};

  auto const light_path = pathfinding.find_path(start, goal, Passability::Light);
  auto const heavy_path = pathfinding.find_path(start, goal, Passability::Heavy);

  ASSERT_FALSE(light_path.empty());
  ASSERT_FALSE(heavy_path.empty());
  EXPECT_EQ(light_path.back(), goal);
  EXPECT_EQ(heavy_path.back(), goal) << "the wood must be skirted, not a dead end";

  auto crosses_forest = [&pathfinding](const std::vector<Game::Systems::Point>& path) {
    return std::any_of(
        path.begin(), path.end(), [&pathfinding](const Game::Systems::Point& cell) {
          return pathfinding.is_forest(cell.x, cell.y);
        });
  };

  EXPECT_TRUE(crosses_forest(light_path)) << "the straight line runs through the wood";
  EXPECT_FALSE(crosses_forest(heavy_path));

  auto widest_deviation = [](const std::vector<Game::Systems::Point>& path) {
    int widest = 0;
    for (const auto& cell : path) {
      widest = std::max(widest, std::abs(cell.x - 20));
    }
    return widest;
  };
  EXPECT_GT(widest_deviation(heavy_path), widest_deviation(light_path));
}

TEST_F(PathfindingTest, ReachableRegionsAgreeWithPathSearch) {
  using Passability = Game::Systems::Pathfinding::Passability;
  constexpr int k_extent = 24;
  constexpr float k_origin = -((static_cast<float>(k_extent) * 0.5F) - 0.5F);

  Game::Systems::Pathfinding pathfinding(k_extent, k_extent);
  pathfinding.set_grid_offset(k_origin, k_origin);
  pathfinding.update_navigation_grid();

  std::uint32_t seed = 0x5EED1234U;
  auto next_random = [&seed]() {
    seed = (seed * 1664525U) + 1013904223U;
    return seed >> 16U;
  };
  for (int y = 0; y < k_extent; ++y) {
    for (int x = 0; x < k_extent; ++x) {
      if (next_random() % 100U < 22U) {
        pathfinding.set_obstacle(x, y, true);
      }
    }
  }

  for (int offset = 2; offset <= 6; ++offset) {
    pathfinding.set_obstacle(offset, 2, true);
    pathfinding.set_obstacle(offset, 6, true);
    pathfinding.set_obstacle(2, offset, true);
    pathfinding.set_obstacle(6, offset, true);
  }
  for (int y = 3; y <= 5; ++y) {
    for (int x = 3; x <= 5; ++x) {
      pathfinding.set_obstacle(x, y, false);
    }
  }

  int compared = 0;
  int unreachable = 0;
  for (int start_y = 0; start_y < k_extent; start_y += 3) {
    for (int start_x = 0; start_x < k_extent; start_x += 3) {
      Game::Systems::Point const start{start_x, start_y};
      if (!pathfinding.is_walkable(start.x, start.y)) {
        continue;
      }
      for (int end_y = 0; end_y < k_extent; end_y += 5) {
        for (int end_x = 0; end_x < k_extent; end_x += 5) {
          Game::Systems::Point const end{end_x, end_y};
          auto const route =
              pathfinding.find_path(start, end, Passability::Light, 0.35F);
          bool const path_arrives = !route.empty() && route.back() == end;
          bool const reachable = pathfinding.can_reach(start, end, Passability::Light);
          EXPECT_EQ(reachable, path_arrives)
              << "(" << start.x << "," << start.y << ") -> (" << end.x << "," << end.y
              << ")";
          ++compared;
          if (!reachable) {
            ++unreachable;
          }
        }
      }
    }
  }

  EXPECT_GT(compared, 100);
  EXPECT_GT(unreachable, 0) << "the sealed chamber must make some pairs unreachable";
}

TEST_F(PathfindingTest, ReachableRegionsFollowGridEdits) {
  using Passability = Game::Systems::Pathfinding::Passability;
  constexpr int k_extent = 9;
  Game::Systems::Pathfinding pathfinding(k_extent, k_extent);
  pathfinding.set_grid_offset(-4.0F, -4.0F);
  pathfinding.update_navigation_grid();

  Game::Systems::Point const left{1, 4};
  Game::Systems::Point const right{7, 4};
  EXPECT_TRUE(pathfinding.can_reach(left, right, Passability::Light));

  for (int y = 0; y < k_extent; ++y) {
    pathfinding.set_obstacle(4, y, true);
  }
  EXPECT_FALSE(pathfinding.can_reach(left, right, Passability::Light));
  EXPECT_NE(pathfinding.region_of(left, Passability::Light),
            Game::Systems::Pathfinding::k_unreachable_region);
  EXPECT_NE(pathfinding.region_of(left, Passability::Light),
            pathfinding.region_of(right, Passability::Light));

  pathfinding.set_obstacle(4, 4, false);
  EXPECT_TRUE(pathfinding.can_reach(left, right, Passability::Light));
  EXPECT_EQ(pathfinding.region_of({4, 0}, Passability::Light),
            Game::Systems::Pathfinding::k_unreachable_region);
}

} // namespace
