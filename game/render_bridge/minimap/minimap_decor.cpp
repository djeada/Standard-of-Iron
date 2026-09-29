#include "minimap_decor.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

#include "minimap_palette.h"
#include "minimap_utils.h"

namespace Game::Map::Minimap {

namespace {

void draw_mountain_symbol(
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

void draw_hill_symbol(
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

void draw_forest_symbol(
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

void draw_compass_rose(QPainter& painter, int width, int height) {

  constexpr float ROSE_SCALE = 0.088F;
  constexpr float ROSE_INSET = 1.62F;
  constexpr float MINOR_RAY = 0.52F;
  constexpr float RAY_WAIST = 0.19F;

  const float radius = static_cast<float>(std::min(width, height)) * ROSE_SCALE;
  if (radius < 6.0F) {
    return;
  }

  const float cx = radius * ROSE_INSET;
  const float cy = static_cast<float>(height) - radius * ROSE_INSET;

  const auto& orient = MinimapOrientation::instance();
  const float north_x = orient.sin_yaw();
  const float north_y = -orient.cos_yaw();

  QColor ring = Palette::INK_LIGHT;
  ring.setAlpha(120);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(ring, 0.9));
  painter.drawEllipse(QPointF(cx, cy), radius, radius);
  painter.drawEllipse(QPointF(cx, cy), radius * 0.24, radius * 0.24);

  QColor minor_ink = Palette::INK_MEDIUM;
  minor_ink.setAlpha(120);
  QColor major_ink = Palette::INK_DARK;
  major_ink.setAlpha(205);

  for (int point = 0; point < 8; ++point) {

    const float turn =
        static_cast<float>(point) * 45.0F * Constants::k_degrees_to_radians;
    const float cos_turn = std::cos(turn);
    const float sin_turn = std::sin(turn);

    const float dir_x = north_x * cos_turn - north_y * sin_turn;
    const float dir_y = north_x * sin_turn + north_y * cos_turn;

    const bool cardinal = (point % 2) == 0;
    const float length = radius * (cardinal ? 1.0F : MINOR_RAY);
    const float waist = radius * RAY_WAIST * (cardinal ? 1.0F : 0.72F);

    const QPointF tip(cx + dir_x * length, cy + dir_y * length);
    const QPointF left(cx - dir_y * waist, cy + dir_x * waist);
    const QPointF right(cx + dir_y * waist, cy - dir_x * waist);
    const QPointF points[3] = {tip, left, right};

    painter.setPen(Qt::NoPen);
    painter.setBrush(point == 0 ? major_ink : minor_ink);
    painter.drawPolygon(points, 3);
  }

  painter.setBrush(major_ink);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(QPointF(cx, cy), radius * 0.11, radius * 0.11);
}

void draw_map_border(QPainter& painter, int width, int height) {

  constexpr float OUTER_MARGIN = 2.0F;
  painter.setPen(QPen(Palette::INK_MEDIUM, 1.25));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(QRectF(OUTER_MARGIN,
                          OUTER_MARGIN,
                          static_cast<float>(width) - OUTER_MARGIN * 2,
                          static_cast<float>(height) - OUTER_MARGIN * 2));
}

void apply_vignette(QPainter& painter, int width, int height) {

  const float radius = static_cast<float>(std::max(width, height)) * 0.75F;
  QRadialGradient vignette(
      static_cast<float>(width) * 0.5F, static_cast<float>(height) * 0.5F, radius);
  vignette.setColorAt(0.0, Qt::transparent);
  vignette.setColorAt(0.55, QColor(60, 45, 30, 10));
  vignette.setColorAt(1.0, QColor(42, 30, 18, 78));

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.fillRect(0, 0, width, height, vignette);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

auto biome_to_base_color(const BiomeSettings& biome) -> QColor {
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

} // namespace

void paint_parchment_background(QImage& image) {
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

void paint_terrain_base(QImage& image, const MapDefinition& map_def) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QColor biome_color = biome_to_base_color(map_def.biome);

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.setOpacity(0.28);
  painter.fillRect(image.rect(), biome_color);
  painter.setOpacity(1.0);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void paint_terrain_features(QImage& image,
                            const MapDefinition& map_def,
                            const MinimapProjection& projection) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto& feature : map_def.terrain) {
    const auto [px, py] = projection.to_pixel(feature.center_x, feature.center_z);

    float pixel_width = projection.to_pixel_size(feature.width);
    float pixel_depth = projection.to_pixel_size(feature.depth);

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

void paint_historical_styling(QImage& image) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  draw_compass_rose(painter, image.width(), image.height());

  draw_map_border(painter, image.width(), image.height());

  apply_vignette(painter, image.width(), image.height());
}

} // namespace Game::Map::Minimap
