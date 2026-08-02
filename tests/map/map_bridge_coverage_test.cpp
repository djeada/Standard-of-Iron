#include <QDir>
#include <QString>
#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <optional>

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

auto load_map(const QString& file_name) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  QString error;
  EXPECT_TRUE(Game::Map::MapLoader::load_from_json_file(
      maps_directory().filePath(file_name), map, &error))
      << file_name.toStdString() << ": " << error.toStdString();
  return map;
}

struct Crossing {
  const Game::Map::RiverSegment* river = nullptr;
  QVector3D point;
};

auto crossing_for(const Game::Map::Bridge& bridge,
                  const std::vector<Game::Map::RiverSegment>& rivers)
    -> std::optional<Crossing> {
  const QVector3D bridge_vec = bridge.end - bridge.start;
  const float bridge_len = std::hypot(bridge_vec.x(), bridge_vec.z());
  if (bridge_len < 1.0e-4F) {
    return std::nullopt;
  }

  for (const auto& river : rivers) {
    if (!Game::Map::bridge_required_half_length_for_river(bridge, river).has_value()) {
      continue;
    }
    const QVector3D river_vec = river.end - river.start;
    const float cross = Game::Map::xz_cross(bridge_vec, river_vec);
    const QVector3D diff = river.start - bridge.start;
    const float t = Game::Map::xz_cross(diff, river_vec) / cross;
    return Crossing{&river, bridge.start + bridge_vec * t};
  }
  return std::nullopt;
}

} // namespace

TEST(MapBridgeCoverageTest, EveryShippedBridgeReachesBankToBank) {
  const QStringList maps = shipped_maps();
  ASSERT_FALSE(maps.isEmpty());

  for (const QString& file_name : maps) {
    const auto map = load_map(file_name);
    for (std::size_t index = 0; index < map.bridges.size(); ++index) {
      const auto& bridge = map.bridges[index];
      const auto crossing = crossing_for(bridge, map.rivers);
      if (!crossing.has_value()) {
        continue;
      }

      const float drawn_water_half =
          crossing->river->width * 0.5F * Game::Map::k_river_drawn_edge_scale;
      const float required = drawn_water_half;

      const QVector3D start_reach = crossing->point - bridge.start;
      const QVector3D end_reach = bridge.end - crossing->point;
      const float start_len = std::hypot(start_reach.x(), start_reach.z());
      const float end_len = std::hypot(end_reach.x(), end_reach.z());

      EXPECT_GT(start_len, required) << file_name.toStdString() << " bridge " << index;
      EXPECT_GT(end_len, required) << file_name.toStdString() << " bridge " << index;
    }
  }
}

TEST(MapBridgeCoverageTest, EveryShippedBridgeSitsSquareToItsRiver) {
  const QStringList maps = shipped_maps();
  ASSERT_FALSE(maps.isEmpty());

  for (const QString& file_name : maps) {
    const auto map = load_map(file_name);
    for (std::size_t index = 0; index < map.bridges.size(); ++index) {
      const auto& bridge = map.bridges[index];
      const auto crossing = crossing_for(bridge, map.rivers);
      if (!crossing.has_value()) {
        continue;
      }

      const QVector3D bridge_vec = bridge.end - bridge.start;
      const QVector3D river_vec = crossing->river->end - crossing->river->start;
      const float bridge_len = std::hypot(bridge_vec.x(), bridge_vec.z());
      const float river_len = std::hypot(river_vec.x(), river_vec.z());
      ASSERT_GT(bridge_len, 1.0e-4F);
      ASSERT_GT(river_len, 1.0e-4F);

      const float alignment = std::abs(Game::Map::xz_cross(bridge_vec, river_vec)) /
                              (bridge_len * river_len);
      EXPECT_NEAR(alignment, 1.0F, 1.0e-3F)
          << file_name.toStdString() << " bridge " << index << " is skew to its river";
    }
  }
}
