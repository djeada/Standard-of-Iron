#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/units/spawn_type.h"

namespace {

using Game::Map::WorldProp;
using Game::Systems::NavGrid;
using Game::Systems::Pathfinding;
using Game::Systems::Point;

constexpr int k_spawn_snap_radius = 4;

auto make_prop(WorldProp::Type type, float x, float z) -> WorldProp {
  WorldProp prop;
  prop.type = type;
  prop.x = x;
  prop.z = z;
  return prop;
}

auto grid_map(int width, int height) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.grid = {.width = width, .height = height, .tile_size = 1.0F};
  map.coordSystem = Game::Map::CoordSystem::Grid;
  return map;
}

auto build_navigation(const Game::Map::MapDefinition& map) -> Pathfinding* {
  Game::Map::TerrainService::instance().initialize(map);
  NavGrid::initialize(map.grid.width, map.grid.height);
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }
  return pathfinder;
}

class WorldPropNavigationTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }
};

TEST_F(WorldPropNavigationTest, CampPropsBlockTheGroundTheyStandOn) {
  auto map = grid_map(65, 65);
  map.world_props = {make_prop(WorldProp::Type::Tent, 32.0F, 32.0F),
                     make_prop(WorldProp::Type::Ruins, 48.0F, 32.0F),
                     make_prop(WorldProp::Type::FireCamp, 16.0F, 32.0F),
                     make_prop(WorldProp::Type::SupplyCart, 32.0F, 48.0F),
                     make_prop(WorldProp::Type::WeaponRack, 48.0F, 48.0F),
                     make_prop(WorldProp::Type::DeadTree, 16.0F, 48.0F),
                     make_prop(WorldProp::Type::MagicShrine, 16.0F, 16.0F)};

  auto* pathfinder = build_navigation(map);
  ASSERT_NE(pathfinder, nullptr);

  EXPECT_FALSE(pathfinder->is_walkable(32, 32)) << "tent";
  EXPECT_FALSE(pathfinder->is_walkable(48, 32)) << "ruins";
  EXPECT_FALSE(pathfinder->is_walkable(16, 32)) << "firecamp";
  EXPECT_FALSE(pathfinder->is_walkable(32, 48)) << "supply cart";
  EXPECT_FALSE(pathfinder->is_walkable(48, 48)) << "weapon rack";
  EXPECT_FALSE(pathfinder->is_walkable(16, 48)) << "dead tree";
  EXPECT_FALSE(pathfinder->is_walkable(16, 16)) << "magic shrine";
}

TEST_F(WorldPropNavigationTest, PropFootprintsFollowTheirGroundRadius) {
  auto map = grid_map(65, 65);
  map.world_props = {make_prop(WorldProp::Type::Tent, 32.0F, 32.0F)};

  auto* pathfinder = build_navigation(map);
  ASSERT_NE(pathfinder, nullptr);

  EXPECT_FALSE(pathfinder->is_walkable(33, 32));
  EXPECT_FALSE(pathfinder->is_walkable(32, 33));

  const float radius = Game::Map::world_prop_ground_radius(WorldProp::Type::Tent, 1.0F);
  const auto reach = static_cast<int>(radius) + 2;
  EXPECT_TRUE(pathfinder->is_walkable(32 + reach, 32))
      << "the footprint spilled past the tent's ground radius of " << radius;
}

TEST_F(WorldPropNavigationTest, PlantsStayUnderfoot) {
  auto map = grid_map(65, 65);
  map.world_props = {make_prop(WorldProp::Type::Plant, 32.0F, 32.0F)};

  auto* pathfinder = build_navigation(map);
  ASSERT_NE(pathfinder, nullptr);
  EXPECT_TRUE(pathfinder->is_walkable(32, 32));
}

TEST_F(WorldPropNavigationTest, PropsNeverCloseARoad) {
  auto map = grid_map(65, 65);
  map.roads = {{{-32.0F, 0.0F, 0.0F}, {32.0F, 0.0F, 0.0F}, 4.0F}};

  map.world_props = {make_prop(WorldProp::Type::Tent, 32.0F, 33.0F)};

  auto* pathfinder = build_navigation(map);
  ASSERT_NE(pathfinder, nullptr);
  EXPECT_TRUE(pathfinder->is_walkable(32, 32)) << "the road cell was built over";
}

TEST_F(WorldPropNavigationTest, HarvestablePropsKeepTheirCellIdentity) {
  auto map = grid_map(65, 65);
  map.world_props = {make_prop(WorldProp::Type::PineTree, 32.0F, 32.0F),
                     make_prop(WorldProp::Type::Boulder, 40.0F, 32.0F),
                     make_prop(WorldProp::Type::IronOre, 48.0F, 32.0F)};

  auto* pathfinder = build_navigation(map);
  ASSERT_NE(pathfinder, nullptr);
  EXPECT_TRUE(pathfinder->is_tree(32, 32));
  EXPECT_TRUE(pathfinder->is_boulder(40, 32));
  EXPECT_TRUE(pathfinder->is_iron_ore(48, 32));
}

class CampaignMapNavigationTest : public ::testing::TestWithParam<const char*> {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }

  struct LoadedMap {
    Game::Map::MapDefinition definition;
    Pathfinding* pathfinder = nullptr;
  };

  static auto load(const QString& file_name) -> LoadedMap {
    LoadedMap loaded;
    const QString path = QStringLiteral("assets/maps/%1").arg(file_name);
    QString error;
    if (!Game::Map::MapLoader::load_from_json_file(path, loaded.definition, &error)) {
      ADD_FAILURE() << "failed to load " << path.toStdString() << ": "
                    << error.toStdString();
      return loaded;
    }
    loaded.pathfinder = build_navigation(loaded.definition);
    return loaded;
  }

  static auto spawn_cell(const Game::Map::MapDefinition& map,
                         const Game::Map::UnitSpawn& spawn) -> Point {
    float world_x = spawn.x;
    float world_z = spawn.z;
    if (map.coordSystem == Game::Map::CoordSystem::Grid) {
      const float tile = std::max(0.0001F, map.grid.tile_size);
      world_x = (spawn.x - (static_cast<float>(map.grid.width) * 0.5F - 0.5F)) * tile;
      world_z = (spawn.z - (static_cast<float>(map.grid.height) * 0.5F - 0.5F)) * tile;
    }
    return NavGrid::world_to_grid(world_x, world_z);
  }
};

TEST_P(CampaignMapNavigationTest, PropsStrandNoUnitSpawn) {
  LoadedMap loaded = load(QString::fromLatin1(GetParam()));
  ASSERT_NE(loaded.pathfinder, nullptr);

  std::vector<Point> cells;
  std::vector<bool> reachable_with_props;
  for (const auto& spawn : loaded.definition.spawns) {
    if (Game::Units::is_building_spawn(spawn.type)) {
      continue;
    }
    const Point cell = spawn_cell(loaded.definition, spawn);
    cells.push_back(cell);
    reachable_with_props.push_back(
        NavGrid::find_nearest_walkable_grid(cell, k_spawn_snap_radius).has_value());
  }
  ASSERT_FALSE(cells.empty());

  Game::Map::MapDefinition without_props = loaded.definition;
  without_props.world_props.clear();
  Game::Map::TerrainService::instance().clear();
  ASSERT_NE(build_navigation(without_props), nullptr);

  for (std::size_t i = 0; i < cells.size(); ++i) {
    const bool reachable_without_props =
        NavGrid::find_nearest_walkable_grid(cells[i], k_spawn_snap_radius).has_value();
    if (!reachable_without_props) {
      continue;
    }
    EXPECT_TRUE(reachable_with_props[i])
        << GetParam() << " lets props strand the spawn at " << cells[i].x << ","
        << cells[i].y;
  }
}

TEST_P(CampaignMapNavigationTest, PropsLeaveMostOfTheMapWalkable) {
  const LoadedMap loaded = load(QString::fromLatin1(GetParam()));
  ASSERT_NE(loaded.pathfinder, nullptr);

  const int width = loaded.definition.grid.width;
  const int height = loaded.definition.grid.height;
  int blocked = 0;
  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width; ++x) {
      if (!loaded.pathfinder->is_walkable(x, z)) {
        ++blocked;
      }
    }
  }

  const float blocked_fraction =
      static_cast<float>(blocked) / static_cast<float>(width * height);
  EXPECT_LT(blocked_fraction, 0.45F)
      << GetParam() << " blocks " << blocked_fraction * 100.0F << "% of its cells";
}

INSTANTIATE_TEST_SUITE_P(CampaignMaps,
                         CampaignMapNavigationTest,
                         ::testing::Values("map_crossing_rhone.json",
                                           "map_crossing_alps.json",
                                           "map_battle_zama.json",
                                           "map_battle_cannae.json"));

} // namespace
