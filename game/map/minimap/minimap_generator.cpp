#include "minimap_generator.h"

#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

#include "minimap_utils.h"

namespace Game::Map::Minimap {

namespace {

namespace Palette {

constexpr QColor PARCHMENT_BASE{235, 220, 190};
constexpr QColor PARCHMENT_DARK{200, 180, 150};
constexpr QColor PARCHMENT_STAIN{180, 160, 130, 24};

constexpr QColor INK_DARK{45, 35, 25};
constexpr QColor INK_MEDIUM{80, 65, 50};
constexpr QColor INK_LIGHT{120, 100, 80};

constexpr QColor MOUNTAIN_SHADOW{70, 52, 38};
constexpr QColor MOUNTAIN_FACE{115, 96, 75};
constexpr QColor MOUNTAIN_HIGHLIGHT{198, 185, 165};
constexpr QColor HILL_BASE{130, 112, 85};

constexpr QColor WATER_DARK{62, 86, 104};
constexpr QColor WATER_MAIN{86, 120, 142};
constexpr QColor WATER_LIGHT{138, 169, 184};
constexpr QColor WATER_WASH{120, 150, 165, 48};
constexpr QColor WATER_GLOW{204, 219, 224, 88};

constexpr QColor FOREST_BASE{73, 108, 61};
constexpr QColor FOREST_DARK{42, 72, 37};

constexpr QColor ROAD_MAIN{112, 82, 47};
constexpr QColor ROAD_SHADOW{69, 49, 27};
constexpr QColor ROAD_HIGHLIGHT{166, 132, 82};

constexpr QColor STRUCTURE_STONE{110, 95, 75};
constexpr QColor STRUCTURE_SHADOW{62, 44, 28};

constexpr QColor TEAM_BLUE{42, 88, 205};
constexpr QColor TEAM_BLUE_DARK{18, 45, 122};
constexpr QColor TEAM_RED{218, 38, 28};
constexpr QColor TEAM_RED_DARK{130, 16, 12};

} // namespace Palette

auto hash_coords(int x, int y, int seed = 0) -> float {
  const int n = x + y * 57 + seed * 131;
  const int shifted = (n << 13) ^ n;
  return 1.0F - static_cast<float>(
                    (shifted * (shifted * shifted * 15731 + 789221) + 1376312589) &
                    0x7fffffff) /
                    1073741824.0F;
}

struct RiverStroke {
  QPointF start;
  QPointF end;
  QPainterPath path;
  float width = 0.0F;
};

struct RiverPool {
  QPointF center;
  float radius = 0.0F;
};

struct RoadLayerPaths {
  QPainterPath shadow;
  QPainterPath surface;
  QPainterPath highlight;
};

auto point_distance(const QPointF& a, const QPointF& b) -> float {
  const auto dx = static_cast<float>(a.x() - b.x());
  const auto dy = static_cast<float>(a.y() - b.y());
  return std::sqrt(dx * dx + dy * dy);
}

auto clamp_point_to_rect(const QPointF& point, const QRectF& rect) -> QPointF {
  return {std::clamp(point.x(), rect.left(), rect.right()),
          std::clamp(point.y(), rect.top(), rect.bottom())};
}

auto distance_to_rect_edge(const QPointF& point, const QRectF& rect) -> float {
  return static_cast<float>(std::min({point.x() - rect.left(),
                                      rect.right() - point.x(),
                                      point.y() - rect.top(),
                                      rect.bottom() - point.y()}));
}

auto normalize_vector(const QPointF& vector) -> QPointF {
  const float length = point_distance(QPointF(0.0, 0.0), vector);
  if (length <= 0.0001F) {
    return {};
  }
  return {vector.x() / length, vector.y() / length};
}

auto ray_rect_intersection(const QPointF& origin,
                           const QPointF& direction,
                           const QRectF& rect) -> std::optional<QPointF> {
  constexpr double epsilon = 0.0001;
  std::optional<QPointF> closest_hit;
  double closest_t = std::numeric_limits<double>::max();

  auto try_candidate = [&](double t, double x, double y) {
    if (t < -epsilon) {
      return;
    }
    if (x < rect.left() - epsilon || x > rect.right() + epsilon ||
        y < rect.top() - epsilon || y > rect.bottom() + epsilon) {
      return;
    }
    if (t < closest_t) {
      closest_t = t;
      closest_hit = QPointF(x, y);
    }
  };

  if (std::abs(direction.x()) > epsilon) {
    const double left_t = (rect.left() - origin.x()) / direction.x();
    try_candidate(left_t, rect.left(), origin.y() + direction.y() * left_t);

    const double right_t = (rect.right() - origin.x()) / direction.x();
    try_candidate(right_t, rect.right(), origin.y() + direction.y() * right_t);
  }

  if (std::abs(direction.y()) > epsilon) {
    const double top_t = (rect.top() - origin.y()) / direction.y();
    try_candidate(top_t, origin.x() + direction.x() * top_t, rect.top());

    const double bottom_t = (rect.bottom() - origin.y()) / direction.y();
    try_candidate(bottom_t, origin.x() + direction.x() * bottom_t, rect.bottom());
  }

  return closest_hit;
}

auto extend_endpoint_to_edge(const QPointF& point,
                             const QPointF& other,
                             const QRectF& rect,
                             float edge_threshold) -> QPointF {
  const QPointF clamped = clamp_point_to_rect(point, rect);
  if (point != clamped) {
    return clamped;
  }

  if (distance_to_rect_edge(clamped, rect) > edge_threshold) {
    return clamped;
  }

  const QPointF direction = normalize_vector(point - other);
  if (direction.isNull()) {
    return clamped;
  }

  const auto hit = ray_rect_intersection(clamped, direction, rect);
  if (!hit.has_value()) {
    return clamped;
  }
  return clamp_point_to_rect(*hit, rect);
}

auto line_segments_intersection(const QPointF& a1,
                                const QPointF& a2,
                                const QPointF& b1,
                                const QPointF& b2) -> std::optional<QPointF> {
  constexpr float epsilon = 0.0001F;

  const QPointF r = a2 - a1;
  const QPointF s = b2 - b1;
  const auto denom = static_cast<float>(r.x() * s.y() - r.y() * s.x());
  if (std::abs(denom) <= epsilon) {
    return std::nullopt;
  }

  const QPointF delta = b1 - a1;
  const float t = static_cast<float>(delta.x() * s.y() - delta.y() * s.x()) / denom;
  const float u = static_cast<float>(delta.x() * r.y() - delta.y() * r.x()) / denom;

  if (t < -epsilon || t > 1.0F + epsilon || u < -epsilon || u > 1.0F + epsilon) {
    return std::nullopt;
  }

  return QPointF(a1.x() + r.x() * t, a1.y() + r.y() * t);
}

void append_river_pool(std::vector<RiverPool>& pools,
                       const QPointF& center,
                       float radius) {
  for (auto& pool : pools) {
    if (point_distance(pool.center, center) <= std::max(pool.radius, radius) * 0.8F) {
      pool.center = QPointF((pool.center.x() + center.x()) * 0.5,
                            (pool.center.y() + center.y()) * 0.5);
      pool.radius = std::max(pool.radius, radius);
      return;
    }
  }
  pools.push_back({center, radius});
}

auto build_river_stroke(const QPointF& start,
                        const QPointF& end,
                        float width,
                        const QRectF& rect) -> RiverStroke {
  RiverStroke stroke;
  stroke.width = width;

  const float edge_threshold = std::max(
      width * 2.4F,
      std::clamp(static_cast<float>(std::min(rect.width(), rect.height())) * 0.12F,
                 12.0F,
                 28.0F));
  stroke.start = extend_endpoint_to_edge(start, end, rect, edge_threshold);
  stroke.end = extend_endpoint_to_edge(end, start, rect, edge_threshold);

  QPainterPath river_path(stroke.start);

  const auto dx = static_cast<float>(stroke.end.x() - stroke.start.x());
  const auto dy = static_cast<float>(stroke.end.y() - stroke.start.y());
  const float length = std::sqrt(dx * dx + dy * dy);

  if (length <= 0.01F) {
    river_path.lineTo(stroke.end);
    stroke.path = river_path;
    return stroke;
  }

  const QPointF dir(dx / length, dy / length);
  const QPointF perp(-dir.y(), dir.x());

  const float control_span = std::clamp(length * 0.24F, width * 1.5F, length * 0.38F);
  const float bend_base =
      std::clamp(width * 0.8F + length * 0.045F, width * 0.45F, length * 0.14F);
  const float bend_a = hash_coords(static_cast<int>(std::lround(stroke.start.x())),
                                   static_cast<int>(std::lround(stroke.start.y())),
                                   static_cast<int>(std::lround(stroke.end.x())));
  const float bend_b =
      hash_coords(static_cast<int>(std::lround(stroke.end.x())),
                  static_cast<int>(std::lround(stroke.end.y())),
                  static_cast<int>(std::lround(stroke.start.y())) + 17);

  const QPointF control_1 =
      stroke.start + dir * control_span + perp * (bend_a * bend_base);
  const QPointF control_2 =
      stroke.end - dir * control_span +
      perp * ((bend_b * bend_base * 0.65F) - bend_a * width * 0.2F);

  river_path.cubicTo(control_1, control_2, stroke.end);
  stroke.path = river_path;
  return stroke;
}

void draw_river_pool(QPainter& painter, const RiverPool& pool) {
  painter.save();
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  painter.setPen(Qt::NoPen);
  painter.setBrush(Palette::WATER_WASH);
  painter.drawEllipse(pool.center, pool.radius * 1.45F, pool.radius * 1.2F);

  painter.setPen(QPen(Palette::WATER_DARK, std::max(1.2F, pool.radius * 0.65F)));
  painter.setBrush(Palette::WATER_MAIN);
  painter.drawEllipse(pool.center, pool.radius, pool.radius * 0.82F);

  painter.setPen(Qt::NoPen);
  QColor const glow = Palette::WATER_GLOW;
  painter.setBrush(glow);
  painter.drawEllipse(QPointF(pool.center.x() - pool.radius * 0.18F,
                              pool.center.y() - pool.radius * 0.12F),
                      pool.radius * 0.45F,
                      pool.radius * 0.26F);
  painter.restore();
}

void draw_river_stroke_pass(QPainter& painter,
                            const QPainterPath& path,
                            const QColor& color,
                            float width) {
  QPen pen(color);
  pen.setWidthF(width);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);
}

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

  QImage image(img_width, img_height, QImage::Format_ARGB32);
  image.fill(Palette::PARCHMENT_BASE);

  render_parchment_background(image);
  render_terrain_base(image, map_def);
  render_roads(image, map_def);
  render_rivers(image, map_def);
  render_terrain_features(image, map_def);
  render_bridges(image, map_def);
  render_structures(image, map_def);
  apply_historical_styling(image);

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

auto MinimapGenerator::world_to_pixel(float world_x,
                                      float world_z,
                                      const GridDefinition& grid) const
    -> std::pair<float, float> {

  const auto& orient = MinimapOrientation::instance();
  const float rotated_x = world_x * orient.cos_yaw() - world_z * orient.sin_yaw();
  const float rotated_z = world_x * orient.sin_yaw() + world_z * orient.cos_yaw();

  const float world_width = grid.width * grid.tile_size;
  const float world_height = grid.height * grid.tile_size;
  const float pixels_per_tile = pixels_per_tile_for(grid);
  const float img_width = grid.width * pixels_per_tile;
  const float img_height = grid.height * pixels_per_tile;

  const float px = (rotated_x + world_width * 0.5F) * (img_width / world_width);
  const float py = (rotated_z + world_height * 0.5F) * (img_height / world_height);

  return {px, py};
}

auto MinimapGenerator::world_to_pixel_size(float world_size,
                                           const GridDefinition& grid) const -> float {

  return (world_size / grid.tile_size) * pixels_per_tile_for(grid);
}

void MinimapGenerator::render_parchment_background(QImage& image) {
  const int BASE_R = Palette::PARCHMENT_BASE.red();
  const int BASE_G = Palette::PARCHMENT_BASE.green();
  const int BASE_B = Palette::PARCHMENT_BASE.blue();

  for (int y = 0; y < image.height(); ++y) {
    auto* scanline = reinterpret_cast<uint32_t*>(image.scanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const float noise = hash_coords(x / 5, y / 5, 42) * 0.045F;

      const int r = std::clamp(BASE_R + static_cast<int>(noise * 20), 0, 255);
      const int g = std::clamp(BASE_G + static_cast<int>(noise * 18), 0, 255);
      const int b = std::clamp(BASE_B + static_cast<int>(noise * 15), 0, 255);

      scanline[x] = qRgba(r, g, b, 255);
    }
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  std::mt19937 rng(12345);
  std::uniform_real_distribution<float> dist_x(0.0F, static_cast<float>(image.width()));
  std::uniform_real_distribution<float> dist_y(0.0F,
                                               static_cast<float>(image.height()));
  std::uniform_real_distribution<float> dist_size(8.0F, 28.0F);
  std::uniform_real_distribution<float> dist_alpha(0.01F, 0.03F);

  const int num_stains = (image.width() * image.height()) / 30000;
  for (int i = 0; i < num_stains; ++i) {
    const float cx = dist_x(rng);
    const float cy = dist_y(rng);
    const float radius = dist_size(rng);
    const float alpha = dist_alpha(rng);

    QRadialGradient stain(cx, cy, radius);
    QColor stain_color = Palette::PARCHMENT_STAIN;
    stain_color.setAlphaF(static_cast<double>(alpha));
    stain.setColorAt(0, stain_color);
    stain.setColorAt(1, Qt::transparent);

    painter.setBrush(stain);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(cx, cy), radius, radius);
  }
}

void MinimapGenerator::render_terrain_base(QImage& image,
                                           const MapDefinition& map_def) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QColor biome_color = biome_to_base_color(map_def.biome);

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.setOpacity(0.28);
  painter.fillRect(image.rect(), biome_color);
  painter.setOpacity(1.0);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void MinimapGenerator::render_terrain_features(QImage& image,
                                               const MapDefinition& map_def) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& feature : map_def.terrain) {
    const auto [px, py] =
        world_to_pixel(feature.center_x, feature.center_z, map_def.grid);

    float pixel_width = world_to_pixel_size(feature.width, map_def.grid);
    float pixel_depth = world_to_pixel_size(feature.depth, map_def.grid);

    constexpr float MIN_FEATURE_SIZE = 4.0F;
    pixel_width = std::max(pixel_width, MIN_FEATURE_SIZE);
    pixel_depth = std::max(pixel_depth, MIN_FEATURE_SIZE);

    if (feature.type == TerrainType::Mountain) {
      draw_mountain_symbol(painter, px, py, pixel_width, pixel_depth);
    } else if (feature.type == TerrainType::Hill) {
      draw_hill_symbol(painter, px, py, pixel_width, pixel_depth);
    } else if (feature.type == TerrainType::Forest) {
      draw_forest_symbol(painter, px, py, pixel_width, pixel_depth);
    } else if (feature.type == TerrainType::River) {
      painter.setBrush(Palette::WATER_MAIN);
      painter.setPen(QPen(Palette::WATER_DARK, 1.0));
      const float half_w = pixel_width * 0.5F;
      const float half_h = pixel_depth * 0.5F;
      painter.drawEllipse(QPointF(px, py), half_w, half_h);
    }
  }
}

void MinimapGenerator::draw_mountain_symbol(
    QPainter& painter, float cx, float cy, float width, float height) {

  const float peak_height = height * 0.6F;
  const float base_width = width * 0.5F;

  QPainterPath shadow_path;
  shadow_path.moveTo(cx, cy - peak_height);
  shadow_path.lineTo(cx - base_width, cy + height * 0.3F);
  shadow_path.lineTo(cx, cy + height * 0.1F);
  shadow_path.closeSubpath();

  painter.setBrush(Palette::MOUNTAIN_SHADOW);
  painter.setPen(Qt::NoPen);
  painter.drawPath(shadow_path);

  QPainterPath lit_path;
  lit_path.moveTo(cx, cy - peak_height);
  lit_path.lineTo(cx + base_width, cy + height * 0.3F);
  lit_path.lineTo(cx, cy + height * 0.1F);
  lit_path.closeSubpath();

  painter.setBrush(Palette::MOUNTAIN_FACE);
  painter.drawPath(lit_path);

  QPainterPath snow_path;
  snow_path.moveTo(cx, cy - peak_height);
  snow_path.lineTo(cx - base_width * 0.3F, cy - peak_height * 0.5F);
  snow_path.lineTo(cx + base_width * 0.2F, cy - peak_height * 0.6F);
  snow_path.closeSubpath();

  painter.setBrush(Palette::MOUNTAIN_HIGHLIGHT);
  painter.drawPath(snow_path);

  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(Palette::INK_MEDIUM, 0.8));

  QPainterPath outline;
  outline.moveTo(cx - base_width, cy + height * 0.3F);
  outline.lineTo(cx, cy - peak_height);
  outline.lineTo(cx + base_width, cy + height * 0.3F);
  painter.drawPath(outline);
}

void MinimapGenerator::draw_hill_symbol(
    QPainter& painter, float cx, float cy, float width, float height) {
  QColor wash = Palette::HILL_BASE;
  wash.setAlpha(62);
  QColor contour = Palette::INK_LIGHT;
  contour.setAlpha(175);

  const QRectF outer(
      cx - width * 0.48F, cy - height * 0.34F, width * 0.96F, height * 0.68F);
  const QRectF inner(
      cx - width * 0.30F, cy - height * 0.21F, width * 0.60F, height * 0.42F);
  painter.setBrush(wash);
  painter.setPen(QPen(contour, 0.8F));
  painter.drawEllipse(outer);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(contour, 0.65F));
  painter.drawEllipse(inner);
}

void MinimapGenerator::draw_forest_symbol(
    QPainter& painter, float cx, float cy, float width, float height) {

  constexpr int JITTER_SEED_X = 123;
  constexpr int JITTER_SEED_Y = 456;

  QColor forest_wash = Palette::FOREST_BASE;
  forest_wash.setAlpha(118);
  QColor forest_edge = Palette::FOREST_DARK;
  forest_edge.setAlpha(185);
  painter.setBrush(forest_wash);
  painter.setPen(QPen(forest_edge, 0.8F));
  painter.drawEllipse(
      QRectF(cx - width * 0.48F, cy - height * 0.48F, width * 0.96F, height * 0.96F));

  const float tree_size = std::clamp(std::min(width, height) * 0.16F, 1.8F, 5.0F);
  const float spacing = tree_size * 2.7F;

  const int cols = std::clamp(static_cast<int>(width / spacing), 2, 5);
  const int rows = std::clamp(static_cast<int>(height / spacing), 2, 5);

  const float start_x = cx - (cols - 1) * spacing * 0.5F;
  const float start_y = cy - (rows - 1) * spacing * 0.5F;

  painter.setBrush(Palette::FOREST_DARK);
  painter.setPen(Qt::NoPen);

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {

      const float jitter_x =
          (hash_coords(
               col + static_cast<int>(cx), row + static_cast<int>(cy), JITTER_SEED_X) *
           0.22F) *
          tree_size;
      const float jitter_y =
          (hash_coords(
               row + static_cast<int>(cx), col + static_cast<int>(cy), JITTER_SEED_Y) *
           0.22F) *
          tree_size;

      const float tx = start_x + col * spacing + jitter_x;
      const float ty = start_y + row * spacing + jitter_y;

      const float tree_h = tree_size * 0.72F;
      const float tree_w = tree_size * 0.42F;

      QPainterPath tree_path;
      tree_path.moveTo(tx, ty - tree_h);
      tree_path.lineTo(tx - tree_w, ty);
      tree_path.lineTo(tx + tree_w, ty);
      tree_path.closeSubpath();

      painter.drawPath(tree_path);
    }
  }
}

void MinimapGenerator::render_rivers(QImage& image, const MapDefinition& map_def) {
  if (map_def.rivers.empty() && map_def.lakes.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  for (const auto& lake : map_def.lakes) {
    const auto [cx, cy] =
        world_to_pixel(lake.center.x(), lake.center.z(), map_def.grid);
    const float half_width =
        std::max(world_to_pixel_size(lake.width, map_def.grid) * 0.5F, 2.0F);
    const float half_depth =
        std::max(world_to_pixel_size(lake.depth, map_def.grid) * 0.5F, 2.0F);
    painter.save();
    painter.translate(cx, cy);
    painter.rotate(lake.rotation_deg);
    painter.setPen(QPen(Palette::WATER_DARK, std::max(1.0F, half_width * 0.06F)));
    painter.setBrush(Palette::WATER_MAIN);
    painter.drawEllipse(QPointF(0.0, 0.0), half_width, half_depth);
    painter.restore();
  }

  const QRectF river_rect(0.0,
                          0.0,
                          static_cast<double>(image.width() - 1),
                          static_cast<double>(image.height() - 1));
  std::vector<RiverStroke> strokes;
  strokes.reserve(map_def.rivers.size());

  for (const auto& river : map_def.rivers) {
    const auto [x1, y1] =
        world_to_pixel(river.start.x(), river.start.z(), map_def.grid);
    const auto [x2, y2] = world_to_pixel(river.end.x(), river.end.z(), map_def.grid);

    float pixel_width = world_to_pixel_size(river.width, map_def.grid);
    pixel_width = std::max(pixel_width, 1.5F);

    strokes.push_back(
        build_river_stroke(QPointF(x1, y1), QPointF(x2, y2), pixel_width, river_rect));
  }

  std::vector<RiverPool> pools;
  for (std::size_t i = 0; i < strokes.size(); ++i) {
    for (std::size_t j = i + 1; j < strokes.size(); ++j) {
      const float pool_radius = std::max(strokes[i].width, strokes[j].width) * 0.95F;

      if (const auto hit = line_segments_intersection(
              strokes[i].start, strokes[i].end, strokes[j].start, strokes[j].end)) {
        append_river_pool(pools, *hit, pool_radius);
      }

      const std::array<std::pair<QPointF, QPointF>, 4> endpoint_pairs{
          {{strokes[i].start, strokes[j].start},
           {strokes[i].start, strokes[j].end},
           {strokes[i].end, strokes[j].start},
           {strokes[i].end, strokes[j].end}}};
      for (const auto& [lhs, rhs] : endpoint_pairs) {
        if (point_distance(lhs, rhs) <= pool_radius) {
          append_river_pool(
              pools,
              QPointF((lhs.x() + rhs.x()) * 0.5, (lhs.y() + rhs.y()) * 0.5),
              pool_radius);
        }
      }
    }
  }

  for (const auto& stroke : strokes) {
    draw_river_stroke_pass(
        painter, stroke.path, Palette::WATER_WASH, stroke.width * 1.75F);
  }

  for (const auto& pool : pools) {
    draw_river_pool(painter, pool);
  }

  for (const auto& stroke : strokes) {
    draw_river_stroke_pass(
        painter, stroke.path, Palette::WATER_DARK, stroke.width * 1.38F);
  }

  for (const auto& stroke : strokes) {
    draw_river_stroke_pass(
        painter, stroke.path, Palette::WATER_MAIN, stroke.width * 1.08F);
  }

  for (const auto& stroke : strokes) {
    draw_river_stroke_pass(painter,
                           stroke.path,
                           Palette::WATER_LIGHT,
                           std::max(stroke.width * 0.26F, 0.8F));
  }
}

void MinimapGenerator::render_roads(QImage& image, const MapDefinition& map_def) {
  if (map_def.roads.empty()) {
    return;
  }

  RoadLayerPaths paths;
  for (const auto& road : map_def.roads) {
    const auto [x1, y1] = world_to_pixel(road.start.x(), road.start.z(), map_def.grid);
    const auto [x2, y2] = world_to_pixel(road.end.x(), road.end.z(), map_def.grid);

    float pixel_width = world_to_pixel_size(road.width, map_def.grid);
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

void MinimapGenerator::render_bridges(QImage& image, const MapDefinition& map_def) {
  if (map_def.bridges.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& bridge : map_def.bridges) {
    const auto [x1, y1] =
        world_to_pixel(bridge.start.x(), bridge.start.z(), map_def.grid);
    const auto [x2, y2] = world_to_pixel(bridge.end.x(), bridge.end.z(), map_def.grid);

    float pixel_width = world_to_pixel_size(bridge.width, map_def.grid);
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

void MinimapGenerator::render_structures(QImage& image, const MapDefinition& map_def) {
  if (map_def.structures.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& structure : map_def.structures) {
    const auto* line = std::get_if<LineStructureGeometry>(&structure.geometry);
    if (line == nullptr) {
      continue;
    }
    const auto [x1, y1] =
        world_to_pixel(line->start.x(), line->start.z(), map_def.grid);
    const auto [x2, y2] = world_to_pixel(line->end.x(), line->end.z(), map_def.grid);
    QColor wall_color = Palette::STRUCTURE_STONE;
    if (structure.player_id == 1) {
      wall_color = Palette::TEAM_BLUE_DARK;
    } else if (structure.player_id == 2) {
      wall_color = Palette::TEAM_RED_DARK;
    }
    painter.setPen(QPen(wall_color,
                        std::max(1.5F, world_to_pixel_size(line->width, map_def.grid)),
                        Qt::SolidLine,
                        Qt::SquareCap));
    painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
  }

  for (const auto& structure : map_def.structures) {
    const auto* point = std::get_if<PointStructureGeometry>(&structure.geometry);
    if (point == nullptr) {
      continue;
    }
    const auto [px, py] =
        world_to_pixel(point->position.x(), point->position.z(), map_def.grid);

    QColor fill_color = Palette::STRUCTURE_STONE;
    QColor border_color = Palette::STRUCTURE_SHADOW;

    if (structure.player_id == 1) {
      fill_color = Palette::TEAM_BLUE;
      border_color = Palette::TEAM_BLUE_DARK;
    } else if (structure.player_id == 2) {
      fill_color = Palette::TEAM_RED;
      border_color = Palette::TEAM_RED_DARK;
    } else if (structure.player_id > 0) {

      const int hue = (structure.player_id * 47 + 30) % 360;
      fill_color.setHsv(hue, 140, 180);
      border_color.setHsv(hue, 180, 100);
    }

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

  const float SIZE = size;
  const float HALF = SIZE * 0.5F;

  painter.setBrush(fill);
  painter.setPen(QPen(border, 1.5));
  painter.drawRect(
      QRectF(cx - HALF * 0.7F, cy - HALF * 0.7F, SIZE * 0.7F, SIZE * 0.7F));

  const float TOWER_SIZE = SIZE * 0.35F;
  const float TOWER_OFFSET = HALF * 0.85F;

  painter.setBrush(fill);
  painter.setPen(QPen(border, 1.0));

  for (int i = 0; i < 4; ++i) {
    const float tx = cx + ((i & 1) != 0 ? TOWER_OFFSET : -TOWER_OFFSET);
    const float ty = cy + ((i & 2) != 0 ? TOWER_OFFSET : -TOWER_OFFSET);
    painter.drawRect(
        QRectF(tx - TOWER_SIZE * 0.5F, ty - TOWER_SIZE * 0.5F, TOWER_SIZE, TOWER_SIZE));
  }

  painter.setBrush(border);
  painter.setPen(Qt::NoPen);
  painter.drawRect(
      QRectF(cx - SIZE * 0.12F, cy + SIZE * 0.15F, SIZE * 0.24F, SIZE * 0.25F));

  const float MERLON_W = SIZE * 0.15F;
  const float MERLON_H = SIZE * 0.12F;
  painter.setBrush(fill);
  painter.setPen(QPen(border, 0.8));

  for (int i = 0; i < 3; ++i) {
    const float mx = cx - SIZE * 0.25F + static_cast<float>(i) * SIZE * 0.25F;
    const float my = cy - HALF * 0.7F - MERLON_H;
    painter.drawRect(QRectF(mx, my, MERLON_W, MERLON_H));
  }
}

void MinimapGenerator::apply_historical_styling(QImage& image) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  draw_map_border(painter, image.width(), image.height());

  apply_vignette(painter, image.width(), image.height());
}

void MinimapGenerator::draw_map_border(QPainter& painter, int width, int height) {

  constexpr float OUTER_MARGIN = 2.0F;
  painter.setPen(QPen(Palette::INK_MEDIUM, 1.25));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(QRectF(OUTER_MARGIN,
                          OUTER_MARGIN,
                          static_cast<float>(width) - OUTER_MARGIN * 2,
                          static_cast<float>(height) - OUTER_MARGIN * 2));
}

void MinimapGenerator::apply_vignette(QPainter& painter, int width, int height) {

  const float radius = static_cast<float>(std::max(width, height)) * 0.75F;
  QRadialGradient vignette(
      static_cast<float>(width) * 0.5F, static_cast<float>(height) * 0.5F, radius);
  vignette.setColorAt(0.0, Qt::transparent);
  vignette.setColorAt(0.78, Qt::transparent);
  vignette.setColorAt(1.0, QColor(60, 45, 30, 22));

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.fillRect(0, 0, width, height, vignette);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

auto MinimapGenerator::biome_to_base_color(const BiomeSettings& biome) -> QColor {
  const auto surface_profile = make_surface_profile(biome);
  const auto& grass = surface_profile.grass_primary;
  QColor base = QColor::fromRgbF(static_cast<double>(grass.x()),
                                 static_cast<double>(grass.y()),
                                 static_cast<double>(grass.z()));

  int h = 0;
  int s = 0;
  int v = 0;
  base.getHsv(&h, &s, &v);
  base.setHsv(h, static_cast<int>(s * 0.4), static_cast<int>(v * 0.85));

  return base;
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
