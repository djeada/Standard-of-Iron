#include "map_preview_generator.h"
#include "../../units/spawn_type.h"
#include "../map_loader.h"
#include "minimap_generator.h"
#include <QColor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QVariantMap>
#include <cmath>

namespace Game::Map::Minimap {

namespace {

constexpr float k_camera_yaw_cos = -0.70710678118F;
constexpr float k_camera_yaw_sin = -0.70710678118F;

constexpr float BASE_SIZE = 16.0F;
constexpr float INNER_SIZE_RATIO = 0.35F;
constexpr float INNER_OFFSET_RATIO = 0.3F;

} // namespace

MapPreviewGenerator::MapPreviewGenerator()
    : m_minimap_generator(std::make_unique<MinimapGenerator>()) {}

MapPreviewGenerator::~MapPreviewGenerator() = default;

auto MapPreviewGenerator::generate_preview(
    const QString &map_path, const QVariantList &player_configs) -> QImage {

  MapDefinition map_def;
  QString error;
  if (!MapLoader::loadFromJsonFile(map_path, map_def, &error)) {
    QImage error_image(200, 200, QImage::Format_RGBA8888);
    error_image.fill(QColor(40, 40, 40));
    return error_image;
  }

  QImage preview = m_minimap_generator->generate(map_def);

  std::vector<PlayerConfig> parsed_configs =
      parse_player_configs(player_configs);
  draw_player_bases(preview, map_def, parsed_configs);

  return preview;
}

auto MapPreviewGenerator::parse_player_configs(
    const QVariantList &configs) const -> std::vector<PlayerConfig> {
  std::vector<PlayerConfig> result;

  for (const QVariant &var : configs) {
    if (!var.canConvert<QVariantMap>()) {
      continue;
    }

    QVariantMap config = var.toMap();
    PlayerConfig player_config;

    if (config.contains("player_id")) {
      player_config.player_id = config["player_id"].toInt();
    }

    if (config.contains("colorHex")) {
      QString color_hex = config["colorHex"].toString();
      player_config.color = QColor(color_hex);
    }

    if (player_config.player_id > 0 && player_config.color.isValid()) {
      result.push_back(player_config);
    }
  }

  return result;
}

void MapPreviewGenerator::draw_player_bases(
    QImage &image, const MapDefinition &map_def,
    const std::vector<PlayerConfig> &player_configs) {

  if (player_configs.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  constexpr float pixels_per_tile = 2.0F;

  for (const auto &spawn : map_def.spawns) {
    if (!Game::Units::is_building_spawn(spawn.type)) {
      continue;
    }

    if (spawn.player_id <= 0) {
      continue;
    }

    QColor player_color;
    for (const auto &config : player_configs) {
      if (config.player_id == spawn.player_id) {
        player_color = config.color;
        break;
      }
    }

    if (!player_color.isValid()) {
      continue;
    }

    const auto [px, py] =
        world_to_pixel(spawn.x, spawn.z, map_def.grid, pixels_per_tile);

    constexpr float HALF = BASE_SIZE * 0.5F;

    QColor border_color = player_color.darker(150);

    painter.setBrush(player_color);
    painter.setPen(QPen(border_color, 2.5));
    painter.drawEllipse(QPointF(px, py), HALF, HALF);

    painter.setBrush(player_color.lighter(130));
    painter.setPen(Qt::NoPen);
    constexpr float INNER_SIZE = BASE_SIZE * INNER_SIZE_RATIO;
    painter.drawEllipse(
        QPointF(px - HALF * INNER_OFFSET_RATIO, py - HALF * INNER_OFFSET_RATIO),
        INNER_SIZE * 0.5F, INNER_SIZE * 0.5F);
  }
}

auto MapPreviewGenerator::world_to_pixel(
    float world_x, float world_z, const GridDefinition &grid,
    float pixels_per_tile) const -> std::pair<float, float> {

  const float rotated_x =
      world_x * k_camera_yaw_cos - world_z * k_camera_yaw_sin;
  const float rotated_z =
      world_x * k_camera_yaw_sin + world_z * k_camera_yaw_cos;

  const float world_width = grid.width * grid.tile_size;
  const float world_height = grid.height * grid.tile_size;
  const float img_width = grid.width * pixels_per_tile;
  const float img_height = grid.height * pixels_per_tile;

  const float px = (rotated_x + world_width * 0.5F) * (img_width / world_width);
  const float py =
      (rotated_z + world_height * 0.5F) * (img_height / world_height);

  return {px, py};
}

} // namespace Game::Map::Minimap
