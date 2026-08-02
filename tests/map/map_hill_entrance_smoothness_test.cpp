#include <QDir>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>

#include "game/map/map_loader.h"
#include "game/map/terrain.h"

namespace {

auto maps_directory() -> QDir {
  QDir dir(QStringLiteral("assets/maps"));
  EXPECT_TRUE(dir.exists()) << dir.absolutePath().toStdString();
  return dir;
}

auto shipped_maps() -> QStringList {
  return maps_directory().entryList(
      {QStringLiteral("map_*.json")}, QDir::Files, QDir::Name);
}

struct EntranceRoughness {
  int entrance_cells = 0;
  float worst_spike = 0.0F;
  float worst_hole = 0.0F;
  int spike_x = -1;
  int spike_z = -1;
  int hole_x = -1;
  int hole_z = -1;
};

auto measure_entrance_roughness(const QString& file_name) -> EntranceRoughness {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      maps_directory().filePath(file_name), map, &error))
      << file_name.toStdString() << ": " << error.toStdString();

  Game::Map::TerrainHeightMap height_map(
      map.grid.width, map.grid.height, map.grid.tile_size);
  height_map.apply_biome_variation(map.biome);
  height_map.build_from_features(map.terrain);

  const int width = height_map.get_width();
  const int height = height_map.get_height();
  const auto& entrances = height_map.getHillEntrances();

  EntranceRoughness roughness;
  for (int z = 1; z < height - 1; ++z) {
    for (int x = 1; x < width - 1; ++x) {
      const auto index = static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(x);
      if (index >= entrances.size() || !entrances[index]) {
        continue;
      }
      ++roughness.entrance_cells;

      const float centre = height_map.get_height_at_grid(x, z);
      float lowest_neighbour = centre;
      float highest_neighbour = centre;
      bool first = true;
      for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dz == 0) {
            continue;
          }
          const float neighbour = height_map.get_height_at_grid(x + dx, z + dz);
          lowest_neighbour = first ? neighbour : std::min(lowest_neighbour, neighbour);
          highest_neighbour =
              first ? neighbour : std::max(highest_neighbour, neighbour);
          first = false;
        }
      }

      if (centre - highest_neighbour > roughness.worst_spike) {
        roughness.worst_spike = centre - highest_neighbour;
        roughness.spike_x = x;
        roughness.spike_z = z;
      }
      if (lowest_neighbour - centre > roughness.worst_hole) {
        roughness.worst_hole = lowest_neighbour - centre;
        roughness.hole_x = x;
        roughness.hole_z = z;
      }
    }
  }
  return roughness;
}

constexpr float k_max_entrance_step = 0.5F;

} // namespace

TEST(MapHillEntranceSmoothnessTest, NoShippedHillEntranceHasSingleCellSpikes) {
  const QStringList maps = shipped_maps();
  ASSERT_FALSE(maps.isEmpty());

  for (const QString& file_name : maps) {
    const auto roughness = measure_entrance_roughness(file_name);
    EXPECT_LE(roughness.worst_spike, k_max_entrance_step)
        << file_name.toStdString() << " has a tooth at (" << roughness.spike_x << ", "
        << roughness.spike_z << ')';
  }
}

TEST(MapHillEntranceSmoothnessTest, NoShippedHillEntranceHasSingleCellHoles) {
  const QStringList maps = shipped_maps();
  ASSERT_FALSE(maps.isEmpty());

  for (const QString& file_name : maps) {
    const auto roughness = measure_entrance_roughness(file_name);
    EXPECT_LE(roughness.worst_hole, k_max_entrance_step)
        << file_name.toStdString() << " has a hole at (" << roughness.hole_x << ", "
        << roughness.hole_z << ')';
  }
}

TEST(MapHillEntranceSmoothnessTest, ShippedMapsStillCarveHillEntrances) {
  int maps_with_entrances = 0;
  for (const QString& file_name : shipped_maps()) {
    if (measure_entrance_roughness(file_name).entrance_cells > 0) {
      ++maps_with_entrances;
    }
  }
  EXPECT_GT(maps_with_entrances, 5);
}
