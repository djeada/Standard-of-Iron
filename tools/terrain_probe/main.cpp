#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "game/map/map_loader.h"
#include "game/map/terrain.h"

namespace {

auto usage() -> int {
  std::fprintf(stderr,
               "usage: terrain_probe <map.json> <out.bin>\n"
               "\n"
               "Builds the map's terrain with the engine's own heightfield pass and\n"
               "writes what an authored-placement audit cannot derive from the JSON:\n"
               "the surface every body actually stands on, and the hill entrance\n"
               "ramps the engine carves outside the authored footprint.\n");
  return 2;
}

} // namespace

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);
  const QStringList args = QCoreApplication::arguments();
  if (args.size() != 3) {
    return usage();
  }

  Game::Map::MapDefinition map;
  QString error;
  if (!Game::Map::MapLoader::load_from_json_file(args.at(1), map, &error)) {
    std::fprintf(stderr, "terrain_probe: %s\n", error.toStdString().c_str());
    return 1;
  }

  Game::Map::TerrainHeightMap height_map(
      map.grid.width, map.grid.height, map.grid.tile_size);
  height_map.apply_biome_variation(map.biome);
  height_map.build_from_features(map.terrain);
  height_map.add_lakes(map.lakes);
  height_map.add_river_segments(map.rivers);
  height_map.add_bridges(map.bridges);

  const int width = height_map.get_width();
  const int height = height_map.get_height();
  const auto cell_count =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  const auto& heights = height_map.get_height_data();
  const auto& entrances = height_map.getHillEntrances();
  const auto& types = height_map.getTerrainTypes();
  if (heights.size() != cell_count) {
    std::fprintf(stderr, "terrain_probe: height grid is not %dx%d\n", width, height);
    return 1;
  }

  std::vector<std::uint8_t> entrance_bytes(cell_count, 0);
  std::vector<std::uint8_t> type_bytes(cell_count, 0);
  for (std::size_t index = 0; index < cell_count; ++index) {
    entrance_bytes[index] = index < entrances.size() && entrances[index] ? 1U : 0U;
    type_bytes[index] =
        index < types.size() ? static_cast<std::uint8_t>(types[index]) : 0U;
  }

  QFile out(args.at(2));
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    std::fprintf(
        stderr, "terrain_probe: cannot write %s\n", args.at(2).toStdString().c_str());
    return 1;
  }

  const QString header =
      QStringLiteral(
          R"({"width":%1,"height":%2,"tile_size":%3,"planes":["f32:height","u8:hill_entrance","u8:terrain_type"]})")
          .arg(width)
          .arg(height)
          .arg(static_cast<double>(height_map.get_tile_size()), 0, 'g', 9);
  out.write(header.toUtf8());
  out.write("\n", 1);
  out.write(reinterpret_cast<const char*>(heights.data()),
            static_cast<qint64>(cell_count * sizeof(float)));
  out.write(reinterpret_cast<const char*>(entrance_bytes.data()),
            static_cast<qint64>(cell_count));
  out.write(reinterpret_cast<const char*>(type_bytes.data()),
            static_cast<qint64>(cell_count));
  out.close();
  return 0;
}
