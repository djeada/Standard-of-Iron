#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <deque>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <vector>

#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/systems/walkability.h"
#include "game/units/spawn_type.h"

namespace {

const char* const k_skirmish_maps[] = {
    "map_rivers.json",
    "map_forest.json",
    "map_mountain.json",
    "map_spanish_grove.json",
    "map_copper_canyons.json",
    "map_amber_delta.json",
};

auto maps_directory() -> QDir {
  QDir dir(QStringLiteral("assets/maps"));
  EXPECT_TRUE(dir.exists()) << dir.absolutePath().toStdString();
  return dir;
}

auto load(const QString& file_name) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      maps_directory().filePath(file_name), map, &error))
      << file_name.toStdString() << ": " << error.toStdString();
  return map;
}

auto build_walkable_terrain(const Game::Map::MapDefinition& map)
    -> Game::Map::TerrainHeightMap {
  Game::Map::TerrainHeightMap height_map(
      map.grid.width, map.grid.height, map.grid.tile_size);
  height_map.apply_biome_variation(map.biome);
  height_map.build_from_features(map.terrain);
  height_map.add_lakes(map.lakes);
  height_map.add_river_segments(map.rivers);
  height_map.add_bridges(map.bridges);
  return height_map;
}

struct GridPoint {
  int x = 0;
  int z = 0;
};

auto to_grid(const Game::Map::MapDefinition& map,
             float world_x,
             float world_z) -> GridPoint {
  const float tile = std::max(map.grid.tile_size, 0.0001F);
  return {
      static_cast<int>(std::lround(world_x / tile + (map.grid.width * 0.5F - 0.5F))),
      static_cast<int>(std::lround(world_z / tile + (map.grid.height * 0.5F - 0.5F)))};
}

auto nearest_walkable(const Game::Map::TerrainHeightMap& terrain,
                      GridPoint origin,
                      int reach) -> GridPoint {
  for (int radius = 0; radius <= reach; ++radius) {
    for (int dz = -radius; dz <= radius; ++dz) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::max(std::abs(dx), std::abs(dz)) != radius) {
          continue;
        }
        const GridPoint candidate{origin.x + dx, origin.z + dz};
        if (terrain.is_walkable(candidate.x, candidate.z)) {
          return candidate;
        }
      }
    }
  }
  return origin;
}

auto flood(const Game::Map::TerrainHeightMap& terrain,
           GridPoint origin) -> std::vector<std::uint8_t> {
  const int width = terrain.get_width();
  const int height = terrain.get_height();
  std::vector<std::uint8_t> seen(
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
  if (!terrain.is_walkable(origin.x, origin.z)) {
    return seen;
  }

  std::deque<GridPoint> queue{origin};
  seen[static_cast<std::size_t>(origin.z) * static_cast<std::size_t>(width) +
       static_cast<std::size_t>(origin.x)] = 1U;
  while (!queue.empty()) {
    const GridPoint cell = queue.front();
    queue.pop_front();
    constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& dir : k_dirs) {
      const GridPoint next{cell.x + dir[0], cell.z + dir[1]};
      if (next.x < 0 || next.z < 0 || next.x >= width || next.z >= height) {
        continue;
      }
      const auto index =
          static_cast<std::size_t>(next.z) * static_cast<std::size_t>(width) +
          static_cast<std::size_t>(next.x);
      if (seen[index] != 0U || !terrain.is_walkable(next.x, next.z)) {
        continue;
      }
      seen[index] = 1U;
      queue.push_back(next);
    }
  }
  return seen;
}

auto player_starts(const Game::Map::MapDefinition& map) -> std::map<int, GridPoint> {
  std::map<int, GridPoint> starts;
  for (const auto& entry : map.structures) {
    if (entry.player_id <= 0 || entry.type != Game::Units::SpawnType::Barracks) {
      continue;
    }
    const auto* point = std::get_if<Game::Map::PointStructureGeometry>(&entry.geometry);
    if (point == nullptr) {
      continue;
    }
    starts.emplace(entry.player_id,
                   to_grid(map, point->position.x(), point->position.z()));
  }
  return starts;
}

auto build_navigation_grid(const Game::Map::MapDefinition& map)
    -> Game::Systems::Pathfinding* {
  Game::Map::TerrainService::instance().initialize(map);
  Game::Systems::NavGrid::initialize(map.grid.width, map.grid.height);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->mark_navigation_grid_dirty();
    pathfinder->update_navigation_grid();
  }
  return pathfinder;
}

constexpr float k_heaviest_body_clearance = 0.45F;

auto world_position(const Game::Map::MapDefinition& map, GridPoint cell) -> QVector3D {
  const float tile = std::max(map.grid.tile_size, 0.0001F);
  return {(static_cast<float>(cell.x) - (map.grid.width * 0.5F - 0.5F)) * tile,
          0.0F,
          (static_cast<float>(cell.z) - (map.grid.height * 0.5F - 0.5F)) * tile};
}

auto standable_cell(const Game::Map::MapDefinition& map,
                    GridPoint origin,
                    Game::Systems::BodyProfile profile)
    -> std::optional<Game::Systems::Point> {
  auto const anchor = Game::Systems::Walkability::nearest_standable(
      world_position(map, origin), profile, 32.0F);
  if (!anchor.has_value()) {
    return std::nullopt;
  }
  return Game::Systems::NavGrid::world_to_grid(anchor->x(), anchor->z());
}

} // namespace

TEST(MapSkirmishReachabilityTest, EveryStartCanWalkToEveryOtherStart) {
  for (const char* file_name : k_skirmish_maps) {
    const QString name = QString::fromLatin1(file_name);
    const auto map = load(name);
    const auto terrain = build_walkable_terrain(map);
    const auto starts = player_starts(map);

    ASSERT_GE(starts.size(), 2U) << name.toStdString() << " has fewer than two starts";

    const auto first = nearest_walkable(terrain, starts.begin()->second, 24);
    const auto reachable = flood(terrain, first);
    for (const auto& [player_id, position] : starts) {
      const auto target = nearest_walkable(terrain, position, 24);
      const auto index = static_cast<std::size_t>(target.z) *
                             static_cast<std::size_t>(terrain.get_width()) +
                         static_cast<std::size_t>(target.x);
      ASSERT_LT(index, reachable.size());
      EXPECT_NE(reachable[index], 0U)
          << name.toStdString() << ": player " << player_id
          << " is walled off from player " << starts.begin()->first;
    }
  }
}

TEST(MapSkirmishReachabilityTest, EveryStartOpensOntoTheRoadNetwork) {
  for (const char* file_name : k_skirmish_maps) {
    const QString name = QString::fromLatin1(file_name);
    const auto map = load(name);
    ASSERT_FALSE(map.roads.empty()) << name.toStdString() << " ships no roads";

    const auto terrain = build_walkable_terrain(map);
    for (const auto& [player_id, position] : player_starts(map)) {
      const auto start = nearest_walkable(terrain, position, 24);
      const auto reachable = flood(terrain, start);

      bool touches_a_road = false;
      for (const auto& road : map.roads) {
        const auto point = to_grid(map, road.start.x(), road.start.z());
        const auto landing = nearest_walkable(terrain, point, 8);
        const auto index = static_cast<std::size_t>(landing.z) *
                               static_cast<std::size_t>(terrain.get_width()) +
                           static_cast<std::size_t>(landing.x);
        if (index < reachable.size() && reachable[index] != 0U) {
          touches_a_road = true;
          break;
        }
      }
      EXPECT_TRUE(touches_a_road)
          << name.toStdString() << ": player " << player_id
          << " cannot reach any road; the camp is sealed in its own pocket";
    }
  }
}

TEST(MapSkirmishReachabilityTest, EveryBarracksCanBeWalkedToOnTheNavigationGrid) {
  for (const char* file_name : k_skirmish_maps) {
    const QString name = QString::fromLatin1(file_name);
    const auto map = load(name);
    auto* pathfinder = build_navigation_grid(map);
    ASSERT_NE(pathfinder, nullptr) << name.toStdString();

    const auto starts = player_starts(map);
    ASSERT_GE(starts.size(), 2U) << name.toStdString() << " has fewer than two starts";

    for (auto const passability : {Game::Systems::Pathfinding::Passability::Light,
                                   Game::Systems::Pathfinding::Passability::Heavy}) {
      Game::Systems::BodyProfile profile;
      profile.radius = k_heaviest_body_clearance;
      profile.passability = passability;

      std::map<int, Game::Systems::Point> anchors;
      for (const auto& [player_id, position] : starts) {
        auto const cell = standable_cell(map, position, profile);
        ASSERT_TRUE(cell.has_value())
            << name.toStdString() << ": player " << player_id
            << " has no standable ground within reach of its barracks";
        anchors.emplace(player_id, *cell);
      }

      for (const auto& [from_id, from_cell] : anchors) {
        for (const auto& [to_id, to_cell] : anchors) {
          if (from_id == to_id) {
            continue;
          }
          auto const route = pathfinder->find_path(
              from_cell, to_cell, passability, k_heaviest_body_clearance);
          ASSERT_FALSE(route.empty())
              << name.toStdString() << ": no route at all from player " << from_id
              << " to player " << to_id;

          EXPECT_TRUE(route.back() == to_cell)
              << name.toStdString() << ": player " << from_id << " cannot reach player "
              << to_id << " with a "
              << (passability == Game::Systems::Pathfinding::Passability::Light
                      ? "light"
                      : "heavy")
              << " body; the route stops at (" << route.back().x << ", "
              << route.back().y << ") instead of (" << to_cell.x << ", " << to_cell.y
              << ")";
        }
      }
    }
    Game::Map::TerrainService::instance().clear();
  }
}

TEST(MapSkirmishReachabilityTest, NoStartIsMarooned) {
  for (const char* file_name : k_skirmish_maps) {
    const QString name = QString::fromLatin1(file_name);
    const auto map = load(name);
    auto* pathfinder = build_navigation_grid(map);
    ASSERT_NE(pathfinder, nullptr) << name.toStdString();

    Game::Systems::BodyProfile profile;
    profile.radius = k_heaviest_body_clearance;

    std::size_t open_cells = 0;
    for (int z = 0; z < map.grid.height; ++z) {
      for (int x = 0; x < map.grid.width; ++x) {
        if (pathfinder->is_walkable(x, z)) {
          ++open_cells;
        }
      }
    }
    ASSERT_GT(open_cells, 0U) << name.toStdString();

    for (const auto& [player_id, position] : player_starts(map)) {
      auto const anchor = standable_cell(map, position, profile);
      ASSERT_TRUE(anchor.has_value()) << name.toStdString() << " player " << player_id;

      std::vector<std::uint8_t> seen(static_cast<std::size_t>(map.grid.width) *
                                         static_cast<std::size_t>(map.grid.height),
                                     0U);
      std::deque<Game::Systems::Point> queue;
      auto index_of = [&map](const Game::Systems::Point& cell) {
        return static_cast<std::size_t>(cell.y) *
                   static_cast<std::size_t>(map.grid.width) +
               static_cast<std::size_t>(cell.x);
      };
      seen[index_of(*anchor)] = 1U;
      queue.push_back(*anchor);
      std::size_t reached = 1;
      while (!queue.empty()) {
        const auto cell = queue.front();
        queue.pop_front();
        constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : k_dirs) {
          const Game::Systems::Point next{cell.x + dir[0], cell.y + dir[1]};
          if (next.x < 0 || next.y < 0 || next.x >= map.grid.width ||
              next.y >= map.grid.height) {
            continue;
          }
          if (seen[index_of(next)] != 0U || !pathfinder->is_walkable(next.x, next.y)) {
            continue;
          }
          seen[index_of(next)] = 1U;
          ++reached;
          queue.push_back(next);
        }
      }

      EXPECT_GT(static_cast<double>(reached) / static_cast<double>(open_cells), 0.4)
          << name.toStdString() << ": player " << player_id << " opens onto only "
          << reached << " of " << open_cells << " walkable cells";
    }
    Game::Map::TerrainService::instance().clear();
  }
}
