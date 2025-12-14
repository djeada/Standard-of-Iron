#include "minimap_generator.h"
#include "minimap_utils.h"
#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>
#include <random>

namespace Game::Map::Minimap {

namespace {

namespace Palette {

constexpr QColor PARCHMENT_BASE{235, 220, 190};
constexpr QColor PARCHMENT_LIGHT{245, 235, 215};
constexpr QColor PARCHMENT_DARK{200, 180, 150};
constexpr QColor PARCHMENT_STAIN{180, 160, 130, 40};

constexpr QColor INK_DARK{45, 35, 25};
constexpr QColor INK_MEDIUM{80, 65, 50};
constexpr QColor INK_LIGHT{120, 100, 80};

constexpr QColor MOUNTAIN_SHADOW{95, 80, 65};
constexpr QColor MOUNTAIN_FACE{140, 125, 105};
constexpr QColor MOUNTAIN_HIGHLIGHT{180, 165, 145};
constexpr QColor HILL_BASE{160, 145, 120};

constexpr QColor WATER_DARK{55, 95, 130};
constexpr QColor WATER_MAIN{75, 120, 160};
constexpr QColor WATER_LIGHT{100, 145, 180};

constexpr QColor ROAD_MAIN{130, 105, 75};
constexpr QColor ROAD_HIGHLIGHT{165, 140, 110};

constexpr QColor STRUCTURE_STONE{160, 150, 135};
constexpr QColor STRUCTURE_SHADOW{100, 85, 70};

constexpr QColor TEAM_BLUE{65, 105, 165};
constexpr QColor TEAM_BLUE_DARK{40, 65, 100};
constexpr QColor TEAM_RED{175, 65, 55};
constexpr QColor TEAM_RED_DARK{110, 40, 35};

} // namespace Palette

auto hash_coords(int x, int y, int seed = 0) -> float {
  const int n = x + y * 57 + seed * 131;
  const int shifted = (n << 13) ^ n;
  return 1.0F -
         static_cast<float>(
             (shifted * (shifted * shifted * 15731 + 789221) + 1376312589) &
             0x7fffffff) /
             1073741824.0F;
}

} // namespace

MinimapGenerator::MinimapGenerator() : m_config() {}

MinimapGenerator::MinimapGenerator(const Config &config) : m_config(config) {}

auto MinimapGenerator::generate(const MapDefinition &map_def) -> QImage {

  const int img_width =
      static_cast<int>(map_def.grid.width * m_config.pixels_per_tile);
  const int img_height =
      static_cast<int>(map_def.grid.height * m_config.pixels_per_tile);

  QImage image(img_width, img_height, QImage::Format_RGBA8888);
  image.fill(Palette::PARCHMENT_BASE);

  render_parchment_background(image);
  render_terrain_base(image, map_def);
  render_terrain_features(image, map_def);
  render_rivers(image, map_def);
  render_roads(image, map_def);
  render_bridges(image, map_def);
  render_structures(image, map_def);
  apply_historical_styling(image);

  return image;
}

auto MinimapGenerator::world_to_pixel(float world_x, float world_z,
                                      const GridDefinition &grid) const
    -> std::pair<float, float> {

  const float rotated_x = world_x * Constants::k_camera_yaw_cos -
                          world_z * Constants::k_camera_yaw_sin;
  const float rotated_z = world_x * Constants::k_camera_yaw_sin +
                          world_z * Constants::k_camera_yaw_cos;

  const float world_width = grid.width * grid.tile_size;
  const float world_height = grid.height * grid.tile_size;
  const float img_width = grid.width * m_config.pixels_per_tile;
  const float img_height = grid.height * m_config.pixels_per_tile;

  const float px = (rotated_x + world_width * 0.5F) * (img_width / world_width);
  const float py =
      (rotated_z + world_height * 0.5F) * (img_height / world_height);

  return {px, py};
}

auto MinimapGenerator::world_to_pixel_size(
    float world_size, const GridDefinition &grid) const -> float {

  return (world_size / grid.tile_size) * m_config.pixels_per_tile;
}

void MinimapGenerator::render_parchment_background(QImage &image) {
  QPainter painter(&image);

  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {

      const float noise = hash_coords(x / 3, y / 3, 42) * 0.08F;

      QColor pixel = Palette::PARCHMENT_BASE;
      int r = pixel.red() + static_cast<int>(noise * 20);
      int g = pixel.green() + static_cast<int>(noise * 18);
      int b = pixel.blue() + static_cast<int>(noise * 15);

      pixel.setRgb(std::clamp(r, 0, 255), std::clamp(g, 0, 255),
                   std::clamp(b, 0, 255));
      image.setPixelColor(x, y, pixel);
    }
  }

  painter.setRenderHint(QPainter::Antialiasing, true);

  std::mt19937 rng(12345);
  std::uniform_real_distribution<float> dist_x(
      0.0F, static_cast<float>(image.width()));
  std::uniform_real_distribution<float> dist_y(
      0.0F, static_cast<float>(image.height()));
  std::uniform_real_distribution<float> dist_size(5.0F, 25.0F);
  std::uniform_real_distribution<float> dist_alpha(0.02F, 0.06F);

  const int num_stains = (image.width() * image.height()) / 8000;
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

void MinimapGenerator::render_terrain_base(QImage &image,
                                           const MapDefinition &map_def) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QColor biome_color = biome_to_base_color(map_def.biome);

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.setOpacity(0.15);
  painter.fillRect(image.rect(), biome_color);
  painter.setOpacity(1.0);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void MinimapGenerator::render_terrain_features(QImage &image,
                                               const MapDefinition &map_def) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto &feature : map_def.terrain) {
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
    }
  }
}

void MinimapGenerator::draw_mountain_symbol(QPainter &painter, float cx,
                                            float cy, float width,
                                            float height) {

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

void MinimapGenerator::draw_hill_symbol(QPainter &painter, float cx, float cy,
                                        float width, float height) {

  const float hill_height = height * 0.35F;
  const float base_width = width * 0.6F;

  QPainterPath hill_path;
  hill_path.moveTo(cx - base_width, cy + hill_height * 0.2F);
  hill_path.quadTo(cx - base_width * 0.3F, cy - hill_height, cx,
                   cy - hill_height);
  hill_path.quadTo(cx + base_width * 0.3F, cy - hill_height, cx + base_width,
                   cy + hill_height * 0.2F);
  hill_path.closeSubpath();

  QLinearGradient gradient(cx - base_width, cy, cx + base_width, cy);
  gradient.setColorAt(0.0, Palette::MOUNTAIN_SHADOW);
  gradient.setColorAt(0.4, Palette::HILL_BASE);
  gradient.setColorAt(1.0, Palette::MOUNTAIN_FACE);

  painter.setBrush(gradient);
  painter.setPen(QPen(Palette::INK_LIGHT, 0.6));
  painter.drawPath(hill_path);
}

void MinimapGenerator::render_rivers(QImage &image,
                                     const MapDefinition &map_def) {
  if (map_def.rivers.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto &river : map_def.rivers) {
    const auto [x1, y1] =
        world_to_pixel(river.start.x(), river.start.z(), map_def.grid);
    const auto [x2, y2] =
        world_to_pixel(river.end.x(), river.end.z(), map_def.grid);

    float pixel_width = world_to_pixel_size(river.width, map_def.grid);
    pixel_width = std::max(pixel_width, 1.5F);

    draw_river_segment(painter, x1, y1, x2, y2, pixel_width);
  }
}

void MinimapGenerator::draw_river_segment(QPainter &painter, float x1, float y1,
                                          float x2, float y2, float width) {

  QPainterPath river_path;
  river_path.moveTo(x1, y1);

  const float dx = x2 - x1;
  const float dy = y2 - y1;
  const float length = std::sqrt(dx * dx + dy * dy);

  if (length > 10.0F) {

    const float mid_x = (x1 + x2) * 0.5F;
    const float mid_y = (y1 + y2) * 0.5F;

    const float perp_x = -dy / length;
    const float perp_y = dx / length;
    const float wave_amount =
        hash_coords(static_cast<int>(x1), static_cast<int>(y1)) * width * 0.5F;

    river_path.quadTo(mid_x + perp_x * wave_amount,
                      mid_y + perp_y * wave_amount, x2, y2);
  } else {
    river_path.lineTo(x2, y2);
  }

  QPen outline_pen(Palette::WATER_DARK);
  outline_pen.setWidthF(width * 1.4F);
  outline_pen.setCapStyle(Qt::RoundCap);
  outline_pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(outline_pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(river_path);

  QPen main_pen(Palette::WATER_MAIN);
  main_pen.setWidthF(width);
  main_pen.setCapStyle(Qt::RoundCap);
  main_pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(main_pen);
  painter.drawPath(river_path);

  if (width > 2.0F) {
    QPen highlight_pen(Palette::WATER_LIGHT);
    highlight_pen.setWidthF(width * 0.4F);
    highlight_pen.setCapStyle(Qt::RoundCap);
    painter.setPen(highlight_pen);
    painter.drawPath(river_path);
  }
}

void MinimapGenerator::render_roads(QImage &image,
                                    const MapDefinition &map_def) {
  if (map_def.roads.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto &road : map_def.roads) {
    const auto [x1, y1] =
        world_to_pixel(road.start.x(), road.start.z(), map_def.grid);
    const auto [x2, y2] =
        world_to_pixel(road.end.x(), road.end.z(), map_def.grid);

    float pixel_width = world_to_pixel_size(road.width, map_def.grid);
    pixel_width = std::max(pixel_width, 1.5F);

    draw_road_segment(painter, x1, y1, x2, y2, pixel_width);
  }
}

void MinimapGenerator::draw_road_segment(QPainter &painter, float x1, float y1,
                                         float x2, float y2, float width) {

  QPen road_pen(Palette::ROAD_MAIN);
  road_pen.setWidthF(width);
  road_pen.setCapStyle(Qt::RoundCap);

  QVector<qreal> dash_pattern;
  dash_pattern << 3.0 << 2.0;
  road_pen.setDashPattern(dash_pattern);

  painter.setPen(road_pen);
  painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));

  const float dx = x2 - x1;
  const float dy = y2 - y1;
  const float length = std::sqrt(dx * dx + dy * dy);

  if (length > 8.0F) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(Palette::ROAD_HIGHLIGHT);

    const int num_dots = static_cast<int>(length / 6.0F);
    for (int i = 1; i < num_dots; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(num_dots);
      const float dot_x = x1 + dx * t;
      const float dot_y = y1 + dy * t;
      painter.drawEllipse(QPointF(dot_x, dot_y), width * 0.25F, width * 0.25F);
    }
  }
}

void MinimapGenerator::render_bridges(QImage &image,
                                      const MapDefinition &map_def) {
  if (map_def.bridges.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto &bridge : map_def.bridges) {
    const auto [x1, y1] =
        world_to_pixel(bridge.start.x(), bridge.start.z(), map_def.grid);
    const auto [x2, y2] =
        world_to_pixel(bridge.end.x(), bridge.end.z(), map_def.grid);

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

void MinimapGenerator::render_structures(QImage &image,
                                         const MapDefinition &map_def) {
  if (map_def.spawns.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto &spawn : map_def.spawns) {
    if (!Game::Units::is_building_spawn(spawn.type)) {
      continue;
    }

    const auto [world_x, world_z] =
        grid_to_world_coords(spawn.x, spawn.z, map_def);
    const auto [px, py] = world_to_pixel(world_x, world_z, map_def.grid);

    QColor fill_color = Palette::STRUCTURE_STONE;
    QColor border_color = Palette::STRUCTURE_SHADOW;

    if (spawn.player_id == 1) {
      fill_color = Palette::TEAM_BLUE;
      border_color = Palette::TEAM_BLUE_DARK;
    } else if (spawn.player_id == 2) {
      fill_color = Palette::TEAM_RED;
      border_color = Palette::TEAM_RED_DARK;
    } else if (spawn.player_id > 0) {

      const int hue = (spawn.player_id * 47 + 30) % 360;
      fill_color.setHsv(hue, 140, 180);
      border_color.setHsv(hue, 180, 100);
    }

    draw_fortress_icon(painter, px, py, fill_color, border_color);
  }
}

void MinimapGenerator::draw_fortress_icon(QPainter &painter, float cx, float cy,
                                          const QColor &fill,
                                          const QColor &border) {

  constexpr float SIZE = 10.0F;
  constexpr float HALF = SIZE * 0.5F;

  painter.setBrush(fill);
  painter.setPen(QPen(border, 1.5));
  painter.drawRect(
      QRectF(cx - HALF * 0.7F, cy - HALF * 0.7F, SIZE * 0.7F, SIZE * 0.7F));

  constexpr float TOWER_SIZE = SIZE * 0.35F;
  constexpr float TOWER_OFFSET = HALF * 0.85F;

  painter.setBrush(fill);
  painter.setPen(QPen(border, 1.0));

  for (int i = 0; i < 4; ++i) {
    const float tx = cx + ((i & 1) != 0 ? TOWER_OFFSET : -TOWER_OFFSET);
    const float ty = cy + ((i & 2) != 0 ? TOWER_OFFSET : -TOWER_OFFSET);
    painter.drawRect(QRectF(tx - TOWER_SIZE * 0.5F, ty - TOWER_SIZE * 0.5F,
                            TOWER_SIZE, TOWER_SIZE));
  }

  painter.setBrush(border);
  painter.setPen(Qt::NoPen);
  painter.drawRect(
      QRectF(cx - SIZE * 0.12F, cy + SIZE * 0.15F, SIZE * 0.24F, SIZE * 0.25F));

  constexpr float MERLON_W = SIZE * 0.15F;
  constexpr float MERLON_H = SIZE * 0.12F;
  painter.setBrush(fill);
  painter.setPen(QPen(border, 0.8));

  for (int i = 0; i < 3; ++i) {
    const float mx = cx - SIZE * 0.25F + static_cast<float>(i) * SIZE * 0.25F;
    const float my = cy - HALF * 0.7F - MERLON_H;
    painter.drawRect(QRectF(mx, my, MERLON_W, MERLON_H));
  }
}

void MinimapGenerator::apply_historical_styling(QImage &image) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  draw_map_border(painter, image.width(), image.height());

  apply_vignette(painter, image.width(), image.height());

  draw_compass_rose(painter, image.width(), image.height());
}

void MinimapGenerator::draw_map_border(QPainter &painter, int width,
                                       int height) {

  constexpr float OUTER_MARGIN = 2.0F;
  constexpr float INNER_MARGIN = 5.0F;

  painter.setPen(QPen(Palette::INK_MEDIUM, 1.5));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(QRectF(OUTER_MARGIN, OUTER_MARGIN,
                          static_cast<float>(width) - OUTER_MARGIN * 2,
                          static_cast<float>(height) - OUTER_MARGIN * 2));

  painter.setPen(QPen(Palette::INK_LIGHT, 0.8));
  painter.drawRect(QRectF(INNER_MARGIN, INNER_MARGIN,
                          static_cast<float>(width) - INNER_MARGIN * 2,
                          static_cast<float>(height) - INNER_MARGIN * 2));
}

void MinimapGenerator::apply_vignette(QPainter &painter, int width,
                                      int height) {

  const float radius = static_cast<float>(std::max(width, height)) * 0.75F;
  QRadialGradient vignette(static_cast<float>(width) * 0.5F,
                           static_cast<float>(height) * 0.5F, radius);
  vignette.setColorAt(0.0, Qt::transparent);
  vignette.setColorAt(0.7, Qt::transparent);
  vignette.setColorAt(1.0, QColor(60, 45, 30, 35));

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.fillRect(0, 0, width, height, vignette);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void MinimapGenerator::draw_compass_rose(QPainter &painter, int width,
                                         int height) {
  const float min_dim = static_cast<float>(std::min(width, height));
  const float margin = std::clamp(min_dim * 0.06F, 12.0F, 32.0F);
  const float SIZE = std::clamp(min_dim * 0.08F, 14.0F, 42.0F);
  const float cx = static_cast<float>(width) - margin;
  const float cy = static_cast<float>(height) - margin;

  const float stroke = std::max(1.2F, SIZE * 0.08F);
  painter.setPen(QPen(Palette::INK_MEDIUM, stroke));
  painter.setBrush(Qt::NoBrush);

  QPainterPath north_arrow;
  north_arrow.moveTo(cx, cy - SIZE);
  north_arrow.lineTo(cx - SIZE * 0.3F, cy);
  north_arrow.lineTo(cx + SIZE * 0.3F, cy);
  north_arrow.closeSubpath();

  painter.setBrush(Palette::INK_DARK);
  painter.drawPath(north_arrow);

  QPainterPath south_arrow;
  south_arrow.moveTo(cx, cy + SIZE);
  south_arrow.lineTo(cx - SIZE * 0.3F, cy);
  south_arrow.lineTo(cx + SIZE * 0.3F, cy);
  south_arrow.closeSubpath();

  painter.setBrush(Palette::PARCHMENT_LIGHT);
  painter.drawPath(south_arrow);

  painter.drawLine(QPointF(cx - SIZE * 0.7F, cy),
                   QPointF(cx + SIZE * 0.7F, cy));

  painter.setBrush(Palette::INK_MEDIUM);
  const float dot_radius = std::max(2.0F, SIZE * 0.2F);
  painter.drawEllipse(QPointF(cx, cy), dot_radius, dot_radius);

  painter.setPen(QPen(Palette::INK_DARK, stroke));
  const float n_half_width = SIZE * 0.35F;
  const float n_left = cx - n_half_width;
  const float n_right = cx + n_half_width;
  const float n_top = cy - SIZE - SIZE * 0.7F;
  const float n_bottom = cy - SIZE - SIZE * 0.15F;

  QPainterPath n_path;
  n_path.moveTo(n_left, n_bottom);
  n_path.lineTo(n_left, n_top);
  n_path.lineTo(n_right, n_bottom);
  n_path.lineTo(n_right, n_top);
  painter.drawPath(n_path);
}

auto MinimapGenerator::biome_to_base_color(const BiomeSettings &biome)
    -> QColor {

  const auto &grass = biome.grass_primary;
  QColor base = QColor::fromRgbF(static_cast<double>(grass.x()),
                                 static_cast<double>(grass.y()),
                                 static_cast<double>(grass.z()));

  int h, s, v;
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
    return Palette::WATER_MAIN;
  case TerrainType::Flat:
  default:
    return Palette::PARCHMENT_DARK;
  }
}

} // namespace Game::Map::Minimap
