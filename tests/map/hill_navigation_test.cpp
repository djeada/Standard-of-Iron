

#include <QDir>
#include <QString>
#include <QVector3D>
#include <queue>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "tests/support/hill_plateaus.h"

namespace {

using Game::Systems::NavGrid;
using Game::Systems::Point;

const char* const k_authored_maps[] = {"map_rivers.json",
                                       "map_forest.json",
                                       "map_mountain.json",
                                       "map_spanish_grove.json",
                                       "map_copper_canyons.json",
                                       "map_amber_delta.json",
                                       "map_sallow_ford.json",
                                       "map_pinewater_cut.json"};

class HillNavigationTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Map::TerrainService::instance().clear(); }
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }

  auto activate(const Game::Map::MapDefinition& map) -> void {
    Game::Map::TerrainService::instance().clear();
    Game::Map::TerrainService::instance().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    ASSERT_NE(NavGrid::get_pathfinder(), nullptr);
    NavGrid::get_pathfinder()->update_navigation_grid();
    m_grid = map.grid.width;
  }

  auto load(const char* file_name) -> Game::Map::MapDefinition {
    Game::Map::MapDefinition map;
    QString error;
    EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
        QDir(QStringLiteral("assets/maps")).filePath(QString::fromLatin1(file_name)),
        map,
        &error))
        << file_name << ": " << error.toStdString();
    return map;
  }

  static auto terrain() -> Game::Map::TerrainService& {
    return Game::Map::TerrainService::instance();
  }

  static auto heights() -> const Game::Map::TerrainHeightMap& {
    return *Game::Map::TerrainService::instance().get_height_map();
  }

  static auto pathfinder() -> Game::Systems::Pathfinding& {
    return *NavGrid::get_pathfinder();
  }

  [[nodiscard]] auto index_of(Point cell) const -> std::size_t {
    return static_cast<std::size_t>(cell.y) * m_grid + cell.x;
  }

  [[nodiscard]] auto in_grid(Point cell) const -> bool {
    return cell.x >= 0 && cell.x < m_grid && cell.y >= 0 && cell.y < m_grid;
  }

  [[nodiscard]] auto highest_route_available(Point from, Point to) const -> float {
    std::vector<float> best(static_cast<std::size_t>(m_grid) * m_grid,
                            -std::numeric_limits<float>::infinity());
    std::priority_queue<std::pair<float, int>> open;
    best[index_of(from)] = heights().get_height_at_grid(from.x, from.y);
    open.emplace(best[index_of(from)], (from.y * m_grid) + from.x);
    while (!open.empty()) {
      auto const [value, packed] = open.top();
      open.pop();
      Point const cell{packed % m_grid, packed / m_grid};
      if (value < best[index_of(cell)]) {
        continue;
      }
      if (cell.x == to.x && cell.y == to.y) {
        return value;
      }
      constexpr int k_dirs[8][2] = {
          {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
      for (auto const& dir : k_dirs) {
        Point const next{cell.x + dir[0], cell.y + dir[1]};
        if (!in_grid(next) || !pathfinder().is_walkable(next.x, next.y)) {
          continue;
        }
        float const candidate =
            std::min(value, heights().get_height_at_grid(next.x, next.y));
        if (candidate <= best[index_of(next)]) {
          continue;
        }
        best[index_of(next)] = candidate;
        open.emplace(candidate, (next.y * m_grid) + next.x);
      }
    }
    return -std::numeric_limits<float>::infinity();
  }

  [[nodiscard]] auto
  lowest_point_on_route(const std::vector<Point>& route) const -> float {
    float lowest = std::numeric_limits<float>::infinity();
    for (auto const& step : route) {
      lowest = std::min(lowest, heights().get_height_at_grid(step.x, step.y));
    }
    return lowest;
  }

  int m_grid{0};
};

TEST_F(HillNavigationTest, RoutesAcrossOneHilltopKeepToTheHighGround) {

  constexpr float k_height_given_up_allowance = 2.0F;

  constexpr float k_nearby_metres = 40.0F;

  for (const char* file_name : k_authored_maps) {
    auto const map = load(file_name);
    activate(map);

    int compared = 0;
    for (auto const& cells : TestSupport::hill_plateaus(m_grid)) {
      if (cells.size() < 40U) {
        continue;
      }
      auto const crown = TestSupport::crown_of(cells, heights());
      if (crown.empty() ||
          heights().get_height_at_grid(crown.front().x, crown.front().y) < 3.0F) {
        continue;
      }

      for (std::size_t i = 0; i < crown.size(); i += 29U) {
        for (std::size_t j = i + 1U; j < crown.size(); j += 37U) {
          Point const from = crown[i];
          Point const to = crown[j];
          if (std::hypot(static_cast<float>(from.x - to.x),
                         static_cast<float>(from.y - to.y)) > k_nearby_metres) {
            continue;
          }
          auto const route = pathfinder().find_path(from, to);
          if (route.empty() || route.back().x != to.x || route.back().y != to.y) {
            continue;
          }
          ++compared;
          float const available = highest_route_available(from, to);
          float const taken = lowest_point_on_route(route);
          EXPECT_GE(taken, available - k_height_given_up_allowance)
              << file_name << ": the route from (" << from.x << ", " << from.y
              << ") to (" << to.x << ", " << to.y << ") on one hilltop dropped to "
              << taken << " m when it could have stayed at " << available << " m";
        }
      }
    }
    EXPECT_GT(compared, 0) << file_name << " offered no hilltop to cross";
  }
}

TEST_F(HillNavigationTest, HillEntrancesNeverOpenWaterOrMountain) {
  for (const char* file_name : k_authored_maps) {
    auto const map = load(file_name);
    activate(map);

    int opened = 0;
    Point worst{};
    for (int z = 0; z < m_grid; ++z) {
      for (int x = 0; x < m_grid; ++x) {
        if (!heights().isHillEntrance(x, z) || terrain().is_walkable(x, z) ||
            !pathfinder().is_walkable(x, z)) {
          continue;
        }
        if (heights().isBridgeCell(x, z) || heights().isBridgeCenterline(x, z)) {
          continue;
        }
        ++opened;
        worst = {x, z};
      }
    }
    EXPECT_EQ(opened, 0) << file_name << ": a hill entrance opened " << opened
                         << " cells the terrain blocks, e.g. (" << worst.x << ", "
                         << worst.y << ")";
  }
}

TEST_F(HillNavigationTest, AMarchAcrossOpenGroundGoesRoundAHillNotOverIt) {
  Game::Map::MapDefinition map;
  map.grid.width = 120;
  map.grid.height = 120;
  map.grid.tile_size = 1.0F;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.biome.procedural_trees_enabled = false;
  map.biome.procedural_boulders_enabled = false;
  map.biome.procedural_iron_ore_enabled = false;

  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.radius = 18.0F;
  hill.height = 9.0F;
  hill.entrances.emplace_back(-20.0F, 0.0F, 0.0F);
  hill.entrances.emplace_back(20.0F, 0.0F, 0.0F);
  map.terrain.push_back(hill);

  activate(map);

  Point const west = NavGrid::world_to_grid(-40.0F, 0.0F);
  Point const east = NavGrid::world_to_grid(40.0F, 0.0F);
  ASSERT_TRUE(pathfinder().is_walkable(west.x, west.y));
  ASSERT_TRUE(pathfinder().is_walkable(east.x, east.y));

  auto const route = pathfinder().find_path(west, east);
  ASSERT_FALSE(route.empty());
  ASSERT_EQ(route.back().x, east.x);
  ASSERT_EQ(route.back().y, east.y);

  float highest = 0.0F;
  for (auto const& step : route) {
    highest = std::max(highest, heights().get_height_at_grid(step.x, step.y));
  }
  EXPECT_LT(highest, 3.0F) << "a march across open ground climbed " << highest
                           << " m over the hill rather than walking round its foot";
}

} // namespace
