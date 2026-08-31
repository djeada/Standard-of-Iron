#include <QDir>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <gtest/gtest.h>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/mission_catalog.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include "game/map/terrain.h"
#include "utils/resource_utils.h"

namespace {

struct GridPoint {
  int x = 0;
  int z = 0;
};

auto load_map(const QString& map_path) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      Utils::Resources::resolve_resource_path(map_path), map, &error))
      << map_path.toStdString() << ": " << error.toStdString();
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

auto world_to_grid(const Game::Map::MapDefinition& map,
                   float world_x,
                   float world_z) -> GridPoint {
  const float tile = std::max(map.grid.tile_size, 0.0001F);
  return {
      static_cast<int>(std::lround(world_x / tile + (map.grid.width * 0.5F - 0.5F))),
      static_cast<int>(std::lround(world_z / tile + (map.grid.height * 0.5F - 0.5F)))};
}

auto authored_cell(const Game::Map::MapDefinition& map,
                   float authored_x,
                   float authored_z) -> GridPoint {
  if (map.coordSystem == Game::Map::CoordSystem::Grid) {
    return {static_cast<int>(std::lround(authored_x)),
            static_cast<int>(std::lround(authored_z))};
  }
  return world_to_grid(map, authored_x, authored_z);
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

auto camp_cell(const Game::Map::MapDefinition& map) -> GridPoint {
  for (const auto& entry : map.structures) {
    if (entry.player_id != 1 || entry.type != Game::Units::SpawnType::Barracks) {
      continue;
    }
    const auto* point = std::get_if<Game::Map::PointStructureGeometry>(&entry.geometry);
    if (point != nullptr) {
      return world_to_grid(map, point->position.x(), point->position.z());
    }
  }
  return {map.grid.width / 2, map.grid.height / 2};
}

struct Objective {
  GridPoint cell;
  std::string what;
};

auto objectives_of(const Game::Map::MapDefinition& map,
                   const Game::Mission::MissionDefinition& mission)
    -> std::vector<Objective> {
  std::vector<Objective> points;

  for (const auto& spawn : map.spawns) {
    points.push_back({authored_cell(map, spawn.x, spawn.z),
                      "map spawn " + Game::Units::spawn_typeToString(spawn.type)});
  }
  for (const auto& prop : map.world_props) {
    if (!Game::Map::is_harvestable_world_prop_type(prop.type)) {
      continue;
    }
    points.push_back(
        {authored_cell(map, prop.x, prop.z),
         "harvestable " + QLatin1String(Game::Map::world_prop_type_to_string(prop.type))
                              .toString()
                              .toStdString()});
  }
  for (const auto& forest : map.forests) {
    points.push_back(
        {authored_cell(map, forest.x, forest.z), "forest " + forest.id.toStdString()});
  }
  for (const auto& zone : map.undead_zones) {
    points.push_back(
        {authored_cell(map, zone.x, zone.z), "undead zone " + zone.id.toStdString()});
  }

  for (const auto& unit : mission.player_setup.starting_units) {
    points.push_back({authored_cell(map, unit.position.x, unit.position.z),
                      "mission start " + unit.type.toStdString()});
  }
  for (const auto& ai_setup : mission.ai_setups) {
    for (const auto& wave : ai_setup.waves) {
      for (const auto& entry : wave.resolved_entry_points()) {
        points.push_back({authored_cell(map, entry.x, entry.z),
                          "wave entry for " + ai_setup.id.toStdString()});
      }
    }
  }

  return points;
}

} // namespace

TEST(MissionMapReachabilityTest, EveryStandaloneMissionCanWalkToItsOwnObjectives) {
  const QVariantList missions = Game::Map::MissionCatalog::standalone_missions();
  ASSERT_FALSE(missions.isEmpty()) << "the Missions menu would be empty";

  for (const QVariant& entry : missions) {
    const QVariantMap row = entry.toMap();
    const QString file_path = row.value(QStringLiteral("file_path")).toString();

    Game::Mission::MissionDefinition mission;
    QString error;
    ASSERT_TRUE(
        Game::Mission::MissionLoader::load_from_json_file(file_path, mission, &error))
        << file_path.toStdString() << ": " << error.toStdString();

    const auto map = load_map(mission.map_path);
    const auto terrain = build_walkable_terrain(map);
    const auto camp = nearest_walkable(terrain, camp_cell(map), 24);
    const auto reachable = flood(terrain, camp);

    for (const auto& objective : objectives_of(map, mission)) {
      const auto landing = nearest_walkable(terrain, objective.cell, 6);
      const auto index = static_cast<std::size_t>(landing.z) *
                             static_cast<std::size_t>(terrain.get_width()) +
                         static_cast<std::size_t>(landing.x);
      ASSERT_LT(index, reachable.size())
          << mission.id.toStdString() << ": " << objective.what << " at ("
          << objective.cell.x << ", " << objective.cell.z << ") is off the map";
      EXPECT_NE(reachable[index], 0U)
          << mission.id.toStdString() << ": " << objective.what << " at ("
          << objective.cell.x << ", " << objective.cell.z
          << ") is walled off from the camp";
    }
  }
}

TEST(MissionMapReachabilityTest, EveryStandaloneMissionMapIsSmallEnoughToRead) {
  const QVariantList missions = Game::Map::MissionCatalog::standalone_missions();
  ASSERT_FALSE(missions.isEmpty());

  constexpr int k_largest_mission_map_side = 128;
  for (const QVariant& entry : missions) {
    const QVariantMap row = entry.toMap();
    EXPECT_LE(row.value(QStringLiteral("map_width")).toInt(),
              k_largest_mission_map_side)
        << row.value(QStringLiteral("mission_id")).toString().toStdString();
    EXPECT_LE(row.value(QStringLiteral("map_height")).toInt(),
              k_largest_mission_map_side)
        << row.value(QStringLiteral("mission_id")).toString().toStdString();
  }
}
