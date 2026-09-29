#include "minimap_generator.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <variant>

#include "minimap_decor.h"
#include "minimap_palette.h"
#include "minimap_projection.h"
#include "minimap_utils.h"
#include "minimap_water_layer.h"

namespace Game::Map::Minimap {

namespace {

struct RoadLayerPaths {
  QPainterPath shadow;
  QPainterPath surface;
  QPainterPath highlight;
};

auto stroke_path(const QPainterPath& path, float width) -> QPainterPath {
  QPainterPathStroker stroker;
  stroker.setWidth(width);
  stroker.setCapStyle(Qt::RoundCap);
  stroker.setJoinStyle(Qt::RoundJoin);
  return stroker.createStroke(path);
}

auto structure_icon_size(Game::Units::SpawnType spawn_type) -> float {
  constexpr float base_size = 10.0F;
  constexpr float large_structure_scale = 1.25F;

  if (spawn_type == Game::Units::SpawnType::Barracks ||
      spawn_type == Game::Units::SpawnType::Home) {
    return base_size * large_structure_scale;
  }
  return base_size;
}

auto authored_to_world(float authored_x,
                       float authored_z,
                       const MapDefinition& map_def) -> std::pair<float, float> {
  if (map_def.coordSystem == CoordSystem::World) {
    return {authored_x, authored_z};
  }
  constexpr float grid_center_offset = 0.5F;
  const float tile = std::max(0.0001F, map_def.grid.tile_size);
  return {
      (authored_x - (map_def.grid.width * grid_center_offset - grid_center_offset)) *
          tile,
      (authored_z - (map_def.grid.height * grid_center_offset - grid_center_offset)) *
          tile};
}

} // namespace

MinimapGenerator::MinimapGenerator() = default;

MinimapGenerator::MinimapGenerator(const Config& config)
    : m_config(config) {
}

auto MinimapGenerator::generate(const MapDefinition& map_def) -> QImage {
  const float pixels_per_tile = pixels_per_tile_for(map_def.grid);
  const int img_width =
      std::max(1, static_cast<int>(std::lround(map_def.grid.width * pixels_per_tile)));
  const int img_height =
      std::max(1, static_cast<int>(std::lround(map_def.grid.height * pixels_per_tile)));
  const MinimapProjection projection(map_def.grid, pixels_per_tile);

  QImage image(img_width, img_height, QImage::Format_ARGB32);
  image.fill(Palette::PARCHMENT_BASE);

  paint_parchment_background(image);
  paint_terrain_base(image, map_def);
  render_roads(image, map_def, projection);
  paint_water_layer(image, map_def, projection);
  paint_terrain_features(image, map_def, projection);
  render_bridges(image, map_def, projection);
  render_undead_zones(image, map_def, projection);
  render_world_props(image, map_def, projection);
  render_structures(image, map_def, projection);
  paint_historical_styling(image);

  return image;
}

auto MinimapGenerator::pixels_per_tile_for(const GridDefinition& grid) const -> float {
  const int largest_grid_dimension = std::max(grid.width, grid.height);
  if (largest_grid_dimension <= 0) {
    return std::max(m_config.pixels_per_tile, 0.01F);
  }

  const float configured = std::max(m_config.pixels_per_tile, 0.01F);
  if (m_config.max_image_dimension <= 0) {
    return configured;
  }
  const float capped = static_cast<float>(m_config.max_image_dimension) /
                       static_cast<float>(largest_grid_dimension);
  return std::min(configured, std::max(capped, 0.01F));
}

void MinimapGenerator::render_roads(QImage& image,
                                    const MapDefinition& map_def,
                                    const MinimapProjection& projection) {
  if (map_def.roads.empty()) {
    return;
  }

  RoadLayerPaths paths;
  for (const auto& road : map_def.roads) {
    const auto [x1, y1] = projection.to_pixel(road.start.x(), road.start.z());
    const auto [x2, y2] = projection.to_pixel(road.end.x(), road.end.z());

    float pixel_width = projection.to_pixel_size(road.width);
    pixel_width = std::max(pixel_width, 1.5F);

    QPainterPath centerline(QPointF(x1, y1));
    centerline.lineTo(x2, y2);

    paths.shadow = paths.shadow.united(stroke_path(centerline, pixel_width + 2.0F));
    paths.surface = paths.surface.united(stroke_path(centerline, pixel_width));
    paths.highlight = paths.highlight.united(
        stroke_path(centerline, std::max(pixel_width * 0.35F, 0.8F)));
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  QColor shadow_color = Palette::ROAD_SHADOW;
  shadow_color.setAlpha(220);
  painter.fillPath(paths.shadow, shadow_color);
  painter.fillPath(paths.surface, Palette::ROAD_MAIN);

  QColor highlight_color = Palette::ROAD_HIGHLIGHT;
  highlight_color.setAlpha(235);
  painter.fillPath(paths.highlight, highlight_color);
}

void MinimapGenerator::draw_road_segment(
    QPainter& painter, float x1, float y1, float x2, float y2, float width) {

  QPen shadow_pen(Palette::ROAD_SHADOW);
  shadow_pen.setWidthF(width + 2.0F);
  shadow_pen.setCapStyle(Qt::RoundCap);
  painter.setPen(shadow_pen);
  painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));

  QPen road_pen(Palette::ROAD_MAIN);
  road_pen.setWidthF(width);
  road_pen.setCapStyle(Qt::RoundCap);
  painter.setPen(road_pen);
  painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));

  QPen highlight_pen(Palette::ROAD_HIGHLIGHT);
  highlight_pen.setWidthF(std::max(width * 0.35F, 0.8F));
  highlight_pen.setCapStyle(Qt::RoundCap);
  painter.setPen(highlight_pen);
  painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
}

void MinimapGenerator::render_bridges(QImage& image,
                                      const MapDefinition& map_def,
                                      const MinimapProjection& projection) {
  if (map_def.bridges.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& bridge : map_def.bridges) {
    const auto [x1, y1] = projection.to_pixel(bridge.start.x(), bridge.start.z());
    const auto [x2, y2] = projection.to_pixel(bridge.end.x(), bridge.end.z());

    float pixel_width = projection.to_pixel_size(bridge.width);
    pixel_width = std::max(pixel_width, 2.0F);

    painter.setPen(QPen(Palette::INK_DARK, 1.0));
    painter.setBrush(Palette::STRUCTURE_STONE);

    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = std::sqrt(dx * dx + dy * dy);

    if (length > 0.01F) {

      const float perp_x = -dy / length * pixel_width * 0.5F;
      const float perp_y = dx / length * pixel_width * 0.5F;

      QPolygonF bridge_poly;
      bridge_poly << QPointF(x1 - perp_x, y1 - perp_y)
                  << QPointF(x1 + perp_x, y1 + perp_y)
                  << QPointF(x2 + perp_x, y2 + perp_y)
                  << QPointF(x2 - perp_x, y2 - perp_y);
      painter.drawPolygon(bridge_poly);

      painter.setPen(QPen(Palette::INK_LIGHT, 0.5));
      const int num_planks = static_cast<int>(length / 3.0F);
      for (int i = 1; i < num_planks; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(num_planks);
        const float plank_x = x1 + dx * t;
        const float plank_y = y1 + dy * t;
        painter.drawLine(QPointF(plank_x - perp_x, plank_y - perp_y),
                         QPointF(plank_x + perp_x, plank_y + perp_y));
      }
    }
  }
}

void MinimapGenerator::render_undead_zones(QImage& image,
                                           const MapDefinition& map_def,
                                           const MinimapProjection& projection) {
  if (map_def.undead_zones.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& zone : map_def.undead_zones) {
    const auto [world_x, world_z] = authored_to_world(zone.x, zone.z, map_def);
    const auto [px, py] = projection.to_pixel(world_x, world_z);
    const float radius =
        std::max(3.0F, projection.to_pixel_size(zone.radius * 2.0F) * 0.5F);

    QColor wash = Palette::UNDEAD_ZONE;
    wash.setAlpha(64);
    QColor edge = Palette::UNDEAD_ZONE;
    edge.setAlpha(170);

    painter.setBrush(wash);
    painter.setPen(QPen(edge, 0.9F, Qt::DashLine));
    painter.drawEllipse(QPointF(px, py), radius, radius);
  }
}

void MinimapGenerator::render_world_props(QImage& image,
                                          const MapDefinition& map_def,
                                          const MinimapProjection& projection) {
  if (map_def.world_props.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& prop : map_def.world_props) {
    if (!is_solid_world_prop_type(prop.type)) {
      continue;
    }
    if (is_tree_world_prop_type(prop.type)) {
      continue;
    }

    const auto [world_x, world_z] = authored_to_world(prop.x, prop.z, map_def);
    const auto [px, py] = projection.to_pixel(world_x, world_z);
    const float radius =
        std::max(1.6F,
                 projection.to_pixel_size(
                     world_prop_ground_bounding_radius(prop.type, prop.scale)));

    painter.setBrush(Palette::LANDMARK_FILL);
    painter.setPen(QPen(Palette::STRUCTURE_SHADOW, 0.8F));
    painter.drawEllipse(QPointF(px, py), radius, radius);
  }
}

void MinimapGenerator::render_structures(QImage& image,
                                         const MapDefinition& map_def,
                                         const MinimapProjection& projection) {
  if (map_def.structures.empty()) {
    return;
  }

  const bool landmarks_only = m_config.structure_bake == StructureBake::LandmarksOnly;

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& structure : map_def.structures) {
    if (landmarks_only) {
      break;
    }
    const auto* line = std::get_if<LineStructureGeometry>(&structure.geometry);
    if (line == nullptr) {
      continue;
    }
    const auto [x1, y1] = projection.to_pixel(line->start.x(), line->start.z());
    const auto [x2, y2] = projection.to_pixel(line->end.x(), line->end.z());
    QColor wall_color = Palette::STRUCTURE_STONE;
    if (structure.player_id == 1) {
      wall_color = Palette::TEAM_BLUE_DARK;
    } else if (structure.player_id == 2) {
      wall_color = Palette::TEAM_RED_DARK;
    }
    painter.setPen(QPen(wall_color,
                        std::max(1.5F, projection.to_pixel_size(line->width)),
                        Qt::SolidLine,
                        Qt::SquareCap));
    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
  }

  for (const auto& structure : map_def.structures) {
    const auto* point = std::get_if<PointStructureGeometry>(&structure.geometry);
    if (point == nullptr) {
      continue;
    }
    if (landmarks_only && structure.type != Game::Units::SpawnType::Barracks) {
      continue;
    }
    const auto [px, py] = projection.to_pixel(point->position.x(), point->position.z());

    const QColor fill_color = Palette::STRUCTURE_STONE;
    const QColor border_color = Palette::STRUCTURE_SHADOW;

    draw_fortress_icon(
        painter, px, py, structure_icon_size(structure.type), fill_color, border_color);
  }
}

void MinimapGenerator::draw_fortress_icon(QPainter& painter,
                                          float cx,
                                          float cy,
                                          float size,
                                          const QColor& fill,
                                          const QColor& border) {

  constexpr float HALF_OF_SIZE = 0.62F;
  const float half = size * HALF_OF_SIZE;

  QPointF points[k_keep_polygon_points];

  QColor shadow = Palette::INK_DARK;
  shadow.setAlpha(70);
  keep_polygon(cx + 0.9F, cy + 0.9F, half, points);
  painter.setPen(Qt::NoPen);
  painter.setBrush(shadow);
  painter.drawPolygon(points, k_keep_polygon_points);

  keep_polygon(cx, cy, half, points);
  painter.setBrush(fill);
  painter.setPen(QPen(border, 1.4));
  painter.drawPolygon(points, k_keep_polygon_points);

  painter.setPen(Qt::NoPen);
  painter.setBrush(border);
  painter.drawRect(QRectF(static_cast<qreal>(cx) - size * 0.09F,
                          static_cast<qreal>(cy) + size * 0.10F,
                          size * 0.18F,
                          size * 0.26F));
}

auto MinimapGenerator::terrain_feature_color(TerrainType type) -> QColor {
  switch (type) {
  case TerrainType::Mountain:
    return Palette::MOUNTAIN_SHADOW;
  case TerrainType::Hill:
    return Palette::HILL_BASE;
  case TerrainType::River:
  case TerrainType::Lake:
    return Palette::WATER_MAIN;
  case TerrainType::Forest:
    return Palette::FOREST_BASE;
  case TerrainType::Flat:
  default:
    return Palette::PARCHMENT_DARK;
  }
}

} // namespace Game::Map::Minimap
