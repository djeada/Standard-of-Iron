#include <QDir>
#include <QFileInfo>
#include <QString>

#include <algorithm>
#include <cmath>
#include <deque>
#include <gtest/gtest.h>
#include <map>
#include <vector>

#include "game/map/map_loader.h"
#include "game/map/terrain.h"
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
