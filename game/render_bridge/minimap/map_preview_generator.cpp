#include "map_preview_generator.h"

#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include "../../map/base_options.h"
#include "../../map/map_loader.h"
#include "../../units/spawn_type.h"
#include "minimap_generator.h"
#include "minimap_utils.h"

namespace Game::Map::Minimap {

namespace {

constexpr float BASE_SIZE = 16.0F;
constexpr float INNER_SIZE_RATIO = 0.35F;
constexpr float INNER_OFFSET_RATIO = 0.3F;
constexpr float FREE_BASE_SIZE = 15.0F;
constexpr float CENTRE_BAND = 0.36F;

auto compass_label(float east_offset, float north_offset) -> QString {
  if (std::hypot(east_offset, north_offset) < CENTRE_BAND) {
    return QCoreApplication::translate("MapBases", "Centre");
  }

  constexpr float k_pi = 3.14159265358979323846F;
  const float angle = std::atan2(north_offset, east_offset) * 180.0F / k_pi;
  const int octant = static_cast<int>(std::lround((angle + 360.0F) / 45.0F)) % 8;
  switch (octant) {
  case 0:
    return QCoreApplication::translate("MapBases", "East");
  case 1:
    return QCoreApplication::translate("MapBases", "North-East");
  case 2:
    return QCoreApplication::translate("MapBases", "North");
  case 3:
    return QCoreApplication::translate("MapBases", "North-West");
  case 4:
    return QCoreApplication::translate("MapBases", "West");
  case 5:
    return QCoreApplication::translate("MapBases", "South-West");
  case 6:
    return QCoreApplication::translate("MapBases", "South");
  default:
    return QCoreApplication::translate("MapBases", "South-East");
  }
}

auto place_name_from_key(const QString& key) -> QString {
  static const QRegularExpression slot_key(QStringLiteral("^p\\d+_barracks$"));
  static const QRegularExpression index_key(QStringLiteral("^structure_\\d+$"));
  if (key.isEmpty() || slot_key.match(key).hasMatch() ||
      index_key.match(key).hasMatch()) {
    return {};
  }

  static const QString suffix = QStringLiteral("_barracks");
  QString trimmed = key;
  if (trimmed.endsWith(suffix)) {
    trimmed.chop(suffix.size());
  }
  const QStringList words = trimmed.split('_', Qt::SkipEmptyParts);
  QStringList titled;
  titled.reserve(words.size());
  for (const QString& word : words) {
    titled.append(word.left(1).toUpper() + word.mid(1));
  }
  return titled.join(' ');
}

} // namespace

MapPreviewGenerator::MapPreviewGenerator()
    : m_minimap_generator(std::make_unique<MinimapGenerator>()) {
}

MapPreviewGenerator::~MapPreviewGenerator() = default;

auto MapPreviewGenerator::generate_preview(
    const QString& map_path, const QVariantList& player_configs) -> QImage {

  MapDefinition map_def;
  QString error;
  if (!MapLoader::load_from_json_file(map_path, map_def, &error)) {
    QImage error_image(200, 200, QImage::Format_ARGB32);
    error_image.fill(QColor(40, 40, 40));
    return error_image;
  }

  MinimapOrientation::instance().set_yaw_degrees(map_def.camera.yaw_deg);

  QImage preview = m_minimap_generator->generate(map_def);

  std::vector<PlayerConfig> parsed_configs = parse_player_configs(player_configs);
  draw_player_bases(preview, map_def, parsed_configs);

  return preview;
}

auto MapPreviewGenerator::parse_player_configs(const QVariantList& configs) const
    -> std::vector<PlayerConfig> {
  std::vector<PlayerConfig> result;

  for (const QVariant& var : configs) {
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

    player_config.base_key = config.value("baseKey").toString();

    if (player_config.player_id > 0 && player_config.color.isValid()) {
      result.push_back(player_config);
    }
  }

  return result;
}

void MapPreviewGenerator::draw_player_bases(
    QImage& image,
    const MapDefinition& map_def,
    const std::vector<PlayerConfig>& player_configs) {

  if (player_configs.empty()) {
    return;
  }

  if (map_def.grid.width <= 0 || image.width() <= 0) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const float pixels_per_tile =
      static_cast<float>(image.width()) / static_cast<float>(map_def.grid.width);

  std::unordered_map<QString, QColor> color_by_base_key;
  std::unordered_map<int, QColor> color_by_authored_owner;
  bool any_base_seated = false;
  for (const auto& config : player_configs) {
    if (!config.color.isValid()) {
      continue;
    }
    if (config.base_key.isEmpty()) {
      color_by_authored_owner.emplace(config.player_id, config.color);
      continue;
    }
    any_base_seated = true;
    color_by_base_key.emplace(config.base_key, config.color);
  }

  std::unordered_map<std::size_t, QString> base_key_by_index;
  for (const auto& option : collect_base_options(map_def)) {
    base_key_by_index.emplace(option.structure_index, option.key);
  }

  for (std::size_t index = 0; index < map_def.structures.size(); ++index) {
    const auto& structure = map_def.structures[index];
    const auto* point = std::get_if<PointStructureGeometry>(&structure.geometry);
    if (point == nullptr) {
      continue;
    }

    const auto key_it = base_key_by_index.find(index);
    const bool is_base = key_it != base_key_by_index.end();

    QColor player_color;
    if (is_base) {
      const auto seated = color_by_base_key.find(key_it->second);
      if (seated != color_by_base_key.end()) {
        player_color = seated->second;
      } else if (!any_base_seated && structure.player_id > 0) {
        const auto authored = color_by_authored_owner.find(structure.player_id);
        if (authored != color_by_authored_owner.end()) {
          player_color = authored->second;
        }
      }
    } else if (structure.player_id > 0) {
      const auto authored = color_by_authored_owner.find(structure.player_id);
      if (authored != color_by_authored_owner.end()) {
        player_color = authored->second;
      }
    }

    const auto [px, py] = world_to_pixel(
        point->position.x(), point->position.z(), map_def.grid, pixels_per_tile);

    if (!player_color.isValid()) {

      if (is_base) {
        constexpr float FREE_HALF = FREE_BASE_SIZE * 0.5F;
        painter.setBrush(QColor(26, 20, 15, 150));
        painter.setPen(QPen(QColor(20, 15, 10, 190), 3.5));
        painter.drawEllipse(QPointF(px, py), FREE_HALF, FREE_HALF);
        painter.setPen(QPen(QColor(240, 220, 180), 2.0));
        painter.drawEllipse(QPointF(px, py), FREE_HALF, FREE_HALF);
      }
      continue;
    }

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
        INNER_SIZE * 0.5F,
        INNER_SIZE * 0.5F);
  }
}

auto MapPreviewGenerator::base_markers(const QString& map_path) -> QVariantList {
  QVariantList markers;

  MapDefinition map_def;
  QString error;
  if (!MapLoader::load_from_json_file(map_path, map_def, &error)) {
    return markers;
  }
  if (map_def.grid.width <= 0 || map_def.grid.height <= 0) {
    return markers;
  }

  MinimapOrientation::instance().set_yaw_degrees(map_def.camera.yaw_deg);

  const float world_width =
      static_cast<float>(map_def.grid.width) * map_def.grid.tile_size;
  const float world_height =
      static_cast<float>(map_def.grid.height) * map_def.grid.tile_size;

  QHash<QString, int> name_uses;
  const std::vector<BaseOption> options = collect_base_options(map_def);
  markers.reserve(static_cast<qsizetype>(options.size()));

  for (const auto& option : options) {
    const auto [nx, ny] = Minimap::world_to_pixel(option.position.x(),
                                                  option.position.z(),
                                                  world_width,
                                                  world_height,
                                                  1.0F,
                                                  1.0F);

    QString name = place_name_from_key(option.key);
    if (name.isEmpty()) {

      name = compass_label(option.position.x() / (world_width * 0.5F),
                           -option.position.z() / (world_height * 0.5F));
    }
    const int uses = ++name_uses[name];
    if (uses > 1) {
      name = QStringLiteral("%1 %2").arg(name).arg(uses);
    }

    QVariantMap marker;
    marker["key"] = option.key;
    marker["name"] = name;
    marker["defaultPlayerId"] = option.default_player_id;
    marker["maxPopulation"] = option.max_population;
    marker["previewX"] = std::clamp(nx, 0.0F, 1.0F);
    marker["previewY"] = std::clamp(ny, 0.0F, 1.0F);
    markers.append(marker);
  }

  return markers;
}

auto MapPreviewGenerator::world_to_pixel(float world_x,
                                         float world_z,
                                         const GridDefinition& grid,
                                         float pixels_per_tile) const
    -> std::pair<float, float> {

  const auto& orient = MinimapOrientation::instance();
  const float rotated_x = world_x * orient.cos_yaw() - world_z * orient.sin_yaw();
  const float rotated_z = world_x * orient.sin_yaw() + world_z * orient.cos_yaw();

  const float world_width = grid.width * grid.tile_size;
  const float world_height = grid.height * grid.tile_size;
  const float img_width = grid.width * pixels_per_tile;
  const float img_height = grid.height * pixels_per_tile;

  const float px = (rotated_x + world_width * 0.5F) * (img_width / world_width);
  const float py = (rotated_z + world_height * 0.5F) * (img_height / world_height);

  return {px, py};
}

} // namespace Game::Map::Minimap
