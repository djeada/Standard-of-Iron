#include "world_backdrop.h"

#include <QBrush>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRadialGradient>
#include <QTemporaryFile>
#include <QTransform>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "game/map/map_loader.h"
#include "game/map/procedural_tree_generation.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"

namespace MapEditor {

namespace {

struct Rgb {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
};

auto rgb_of(const QVector3D& v) -> Rgb {
  return {v.x(), v.y(), v.z()};
}

auto mix(const Rgb& a, const Rgb& b, float t) -> Rgb {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

auto scaled(const Rgb& a, float s) -> Rgb {
  return {a.r * s, a.g * s, a.b * s};
}

auto smoothstep(float edge0, float edge1, float x) -> float {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto hash_cell(int x, int y, std::uint32_t seed) -> float {
  auto h = static_cast<std::uint32_t>(x) * 374761393U +
           static_cast<std::uint32_t>(y) * 668265263U + seed * 2246822519U;
  h = (h ^ (h >> 13U)) * 1274126177U;
  h ^= h >> 16U;
  return static_cast<float>(h & 0xFFFFFFU) / static_cast<float>(0x1000000U);
}

auto value_noise(float x, float y, std::uint32_t seed) -> float {
  const float fx = std::floor(x);
  const float fy = std::floor(y);
  const int ix = static_cast<int>(fx);
  const int iy = static_cast<int>(fy);
  float tx = x - fx;
  float ty = y - fy;
  tx = tx * tx * (3.0F - 2.0F * tx);
  ty = ty * ty * (3.0F - 2.0F * ty);
  const float a = hash_cell(ix, iy, seed);
  const float b = hash_cell(ix + 1, iy, seed);
  const float c = hash_cell(ix, iy + 1, seed);
  const float d = hash_cell(ix + 1, iy + 1, seed);
  return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

auto fbm(float x, float y, std::uint32_t seed, int octaves = 3) -> float {
  float total = 0.0F;
  float weight = 0.5F;
  float norm = 0.0F;
  for (int octave = 0; octave < octaves; ++octave) {
    total +=
        value_noise(x, y, seed + static_cast<std::uint32_t>(octave) * 101U) * weight;
    norm += weight;
    x *= 2.03F;
    y *= 2.03F;
    weight *= 0.5F;
  }
  return total / norm;
}

struct Field {
  int width = 0;
  int height = 0;
  std::vector<float> values;

  Field() = default;
  Field(int w, int h, float fill = 0.0F)
      : width(w)
      , height(h)
      , values(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill) {}

  [[nodiscard]] auto at(int x, int y) const -> float {
    x = std::clamp(x, 0, width - 1);
    y = std::clamp(y, 0, height - 1);
    return values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(x)];
  }

  auto ref(int x, int y) -> float& {
    return values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                  static_cast<std::size_t>(x)];
  }

  [[nodiscard]] auto sample_smooth(float x, float y) const -> float {
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const int ix = static_cast<int>(fx);
    const int iy = static_cast<int>(fy);
    const auto weights = [](float t) {
      const float t2 = t * t;
      const float t3 = t2 * t;
      return std::array<float, 4>{(1.0F - 3.0F * t + 3.0F * t2 - t3) / 6.0F,
                                  (4.0F - 6.0F * t2 + 3.0F * t3) / 6.0F,
                                  (1.0F + 3.0F * t + 3.0F * t2 - 3.0F * t3) / 6.0F,
                                  t3 / 6.0F};
    };
    const auto wx = weights(x - fx);
    const auto wy = weights(y - fy);
    float total = 0.0F;
    for (int row = 0; row < 4; ++row) {
      float line = 0.0F;
      for (int column = 0; column < 4; ++column) {
        line +=
            at(ix - 1 + column, iy - 1 + row) * wx[static_cast<std::size_t>(column)];
      }
      total += line * wy[static_cast<std::size_t>(row)];
    }
    return total;
  }

  [[nodiscard]] auto sample(float x, float y) const -> float {
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const int ix = static_cast<int>(fx);
    const int iy = static_cast<int>(fy);
    const float tx = x - fx;
    const float ty = y - fy;
    const float a = at(ix, iy);
    const float b = at(ix + 1, iy);
    const float c = at(ix, iy + 1);
    const float d = at(ix + 1, iy + 1);
    const float top = a + (b - a) * tx;
    const float bottom = c + (d - c) * tx;
    return top + (bottom - top) * ty;
  }
};

void soften(Field& field, int passes) {
  Field scratch(field.width, field.height);
  for (int pass = 0; pass < passes; ++pass) {
    for (int y = 0; y < field.height; ++y) {
      for (int x = 0; x < field.width; ++x) {
        scratch.ref(x, y) =
            (field.at(x - 1, y) + field.at(x, y) * 2.0F + field.at(x + 1, y)) * 0.25F;
      }
    }
    for (int y = 0; y < field.height; ++y) {
      for (int x = 0; x < field.width; ++x) {
        field.ref(x, y) =
            (scratch.at(x, y - 1) + scratch.at(x, y) * 2.0F + scratch.at(x, y + 1)) *
            0.25F;
      }
    }
  }
}

void chamfer(Field& field) {
  constexpr float k_straight = 1.0F;
  constexpr float k_diagonal = 1.41421356F;
  const int w = field.width;
  const int h = field.height;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float best = field.ref(x, y);
      if (x > 0) {
        best = std::min(best, field.ref(x - 1, y) + k_straight);
      }
      if (y > 0) {
        best = std::min(best, field.ref(x, y - 1) + k_straight);
        if (x > 0) {
          best = std::min(best, field.ref(x - 1, y - 1) + k_diagonal);
        }
        if (x + 1 < w) {
          best = std::min(best, field.ref(x + 1, y - 1) + k_diagonal);
        }
      }
      field.ref(x, y) = best;
    }
  }
  for (int y = h - 1; y >= 0; --y) {
    for (int x = w - 1; x >= 0; --x) {
      float best = field.ref(x, y);
      if (x + 1 < w) {
        best = std::min(best, field.ref(x + 1, y) + k_straight);
      }
      if (y + 1 < h) {
        best = std::min(best, field.ref(x, y + 1) + k_straight);
        if (x + 1 < w) {
          best = std::min(best, field.ref(x + 1, y + 1) + k_diagonal);
        }
        if (x > 0) {
          best = std::min(best, field.ref(x - 1, y + 1) + k_diagonal);
        }
      }
      field.ref(x, y) = best;
    }
  }
}

auto to_qcolor(const Rgb& c, int alpha = 255) -> QColor {
  return {std::clamp(static_cast<int>(std::lround(c.r * 255.0F)), 0, 255),
          std::clamp(static_cast<int>(std::lround(c.g * 255.0F)), 0, 255),
          std::clamp(static_cast<int>(std::lround(c.b * 255.0F)), 0, 255),
          alpha};
}

auto develop(const Rgb& c) -> Rgb {
  const auto curve = [](float v) {
    v = std::max(v, 0.0F) * 1.62F;
    return std::pow(v / (1.0F + 0.42F * v), 0.92F);
  };
  return {curve(c.r), curve(c.g), curve(c.b)};
}

auto saturate(const Rgb& c, float amount) -> Rgb {
  const float grey = c.r * 0.299F + c.g * 0.587F + c.b * 0.114F;
  return {grey + (c.r - grey) * amount,
          grey + (c.g - grey) * amount,
          grey + (c.b - grey) * amount};
}

constexpr float k_light_x = -0.55F;
constexpr float k_light_y = -0.60F;
constexpr float k_light_z = 0.58F;

struct MapFrame {
  int width = 0;
  int height = 0;
  float tile = 1.0F;
  float ppc = 1.0F;
  bool grid_coords = true;

  [[nodiscard]] auto grid_of_world(float world, int cells) const -> float {
    return world / tile + (static_cast<float>(cells) * 0.5F - 0.5F);
  }

  [[nodiscard]] auto image_point(float grid_x, float grid_z) const -> QPointF {
    return {(static_cast<float>(width) - grid_x) * ppc,
            (static_cast<float>(height) - grid_z) * ppc};
  }

  [[nodiscard]] auto image_point_of_world(const QVector3D& world) const -> QPointF {
    return image_point(grid_of_world(world.x(), width),
                       grid_of_world(world.z(), height));
  }
};

void stroke_segments(QPainter& painter,
                     const std::vector<std::pair<QPointF, QPointF>>& segments,
                     const std::vector<float>& widths,
                     float width_scale,
                     float width_bias,
                     const QColor& color) {
  for (std::size_t index = 0; index < segments.size(); ++index) {
    painter.setPen(QPen(color,
                        std::max(widths[index] * width_scale + width_bias, 0.6F),
                        Qt::SolidLine,
                        Qt::RoundCap,
                        Qt::RoundJoin));
    painter.drawLine(segments[index].first, segments[index].second);
  }
}

void paint_roads(QPainter& painter,
                 const Game::Map::MapDefinition& map,
                 const MapFrame& frame,
                 const Rgb& soil) {
  if (map.roads.empty()) {
    return;
  }
  std::vector<std::pair<QPointF, QPointF>> segments;
  std::vector<float> widths;
  segments.reserve(map.roads.size());
  widths.reserve(map.roads.size());
  for (const auto& road : map.roads) {
    segments.emplace_back(frame.image_point_of_world(road.start),
                          frame.image_point_of_world(road.end));
    widths.push_back(std::max(road.width / frame.tile, 1.0F) * frame.ppc);
  }
  const Rgb earth = mix(scaled(develop(soil), 1.25F), Rgb{0.63F, 0.53F, 0.39F}, 0.62F);
  stroke_segments(painter,
                  segments,
                  widths,
                  1.0F,
                  frame.ppc * 0.9F,
                  to_qcolor(scaled(earth, 0.55F), 70));
  stroke_segments(
      painter, segments, widths, 0.92F, 0.0F, to_qcolor(scaled(earth, 0.86F), 235));
  stroke_segments(painter, segments, widths, 0.62F, 0.0F, to_qcolor(earth, 235));
  stroke_segments(
      painter, segments, widths, 0.16F, 0.0F, to_qcolor(scaled(earth, 1.16F), 150));
}

void paint_bridges(QPainter& painter,
                   const Game::Map::MapDefinition& map,
                   const MapFrame& frame) {
  for (const auto& bridge : map.bridges) {
    const QPointF a = frame.image_point_of_world(bridge.start);
    const QPointF b = frame.image_point_of_world(bridge.end);
    const QPointF span = b - a;
    const auto length = static_cast<float>(std::hypot(span.x(), span.y()));
    if (length < 0.5F) {
      continue;
    }
    const QPointF along = span / length;
    const QPointF across(-along.y(), along.x());
    const float half = std::max(bridge.width / frame.tile, 2.0F) * frame.ppc * 0.5F;
    const QPointF overhang = along * (frame.ppc * 0.9F);

    const auto deck = [&](float grow, const QPointF& shift) {
      QPolygonF polygon;
      polygon << (a - overhang + across * (half + grow) + shift)
              << (b + overhang + across * (half + grow) + shift)
              << (b + overhang - across * (half + grow) + shift)
              << (a - overhang - across * (half + grow) + shift);
      return polygon;
    };

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(8, 14, 22, 85));
    painter.drawPolygon(
        deck(frame.ppc * 0.15F, QPointF(frame.ppc * 0.55F, frame.ppc * 0.7F)));
    painter.setBrush(QColor(186, 176, 156));
    painter.drawPolygon(deck(0.0F, QPointF()));
    painter.setPen(
        QPen(QColor(158, 138, 108, 150), half * 0.9F, Qt::SolidLine, Qt::FlatCap));
    painter.drawLine(a - overhang, b + overhang);

    painter.setPen(QPen(QColor(150, 140, 122, 150), std::max(frame.ppc * 0.08F, 0.6F)));
    const float step = std::max(frame.ppc * 0.9F, 3.0F);
    for (float d = step * 0.5F; d < length; d += step) {
      const QPointF at = a + along * d;
      painter.drawLine(at + across * (half * 0.86F), at - across * (half * 0.86F));
    }
    painter.setPen(QPen(QColor(112, 104, 92),
                        std::max(frame.ppc * 0.28F, 1.0F),
                        Qt::SolidLine,
                        Qt::FlatCap));
    for (const float side : {-1.0F, 1.0F}) {
      painter.drawLine(a - overhang + across * (half * side),
                       b + overhang + across * (half * side));
    }
    painter.setPen(QPen(QColor(226, 218, 200, 190), std::max(frame.ppc * 0.09F, 0.6F)));
    for (const float side : {-1.0F, 1.0F}) {
      const QPointF lift =
          across * (half * side) - QPointF(frame.ppc * 0.1F, frame.ppc * 0.1F);
      painter.drawLine(a - overhang + lift, b + overhang + lift);
    }
  }
}

auto sprite_of(Game::Map::WorldProp::Type type) -> NaturalSprite {
  using Type = Game::Map::WorldProp::Type;
  switch (type) {
  case Type::PineTree:
    return NaturalSprite::Pine;
  case Type::OliveTree:
    return NaturalSprite::Olive;
  case Type::CypressTree:
    return NaturalSprite::Cypress;
  case Type::PalmTree:
    return NaturalSprite::Palm;
  case Type::DeadTree:
    return NaturalSprite::DeadTree;
  case Type::Boulder:
    return NaturalSprite::Boulder;
  case Type::IronOre:
    return NaturalSprite::IronOre;
  case Type::Plant:
    return NaturalSprite::Plant;
  default:
    return NaturalSprite::None;
  }
}

auto unit_random(unsigned seed, unsigned salt) -> float {
  return hash_cell(static_cast<int>(seed & 0xFFFFU),
                   static_cast<int>((seed >> 16U) & 0xFFFFU),
                   salt);
}

auto blob(const QPointF& centre, float radius, unsigned seed, int points, float wobble)
    -> QPolygonF {
  QPolygonF polygon;
  const float turn = unit_random(seed, 3U) * std::numbers::pi_v<float> * 2.0F;
  for (int index = 0; index < points; ++index) {
    const float angle = turn + static_cast<float>(index) / static_cast<float>(points) *
                                   std::numbers::pi_v<float> * 2.0F;
    const float reach =
        radius *
        (1.0F - wobble +
         wobble * 2.0F * unit_random(seed, 11U + static_cast<unsigned>(index)));
    polygon << (centre + QPointF(std::cos(angle) * reach, std::sin(angle) * reach));
  }
  return polygon;
}

} // namespace

auto natural_sprite_for(const QString& prop_type) -> NaturalSprite {
  if (prop_type == QLatin1String("pine_tree")) {
    return NaturalSprite::Pine;
  }
  if (prop_type == QLatin1String("olive_tree")) {
    return NaturalSprite::Olive;
  }
  if (prop_type == QLatin1String("cypress_tree")) {
    return NaturalSprite::Cypress;
  }
  if (prop_type == QLatin1String("palm_tree")) {
    return NaturalSprite::Palm;
  }
  if (prop_type == QLatin1String("dead_tree")) {
    return NaturalSprite::DeadTree;
  }
  if (prop_type == QLatin1String("boulder")) {
    return NaturalSprite::Boulder;
  }
  if (prop_type == QLatin1String("iron_ore")) {
    return NaturalSprite::IronOre;
  }
  if (prop_type == QLatin1String("plant")) {
    return NaturalSprite::Plant;
  }
  return NaturalSprite::None;
}

void paint_ground_shadow(QPainter& painter, const QPointF& centre, float radius_px) {
  painter.save();
  painter.setPen(Qt::NoPen);
  const QPointF at = centre + QPointF(radius_px * 0.3F, radius_px * 0.4F);
  painter.setBrush(QColor(0, 0, 0, 46));
  painter.drawEllipse(at, radius_px * 1.3F, radius_px * 1.14F);
  painter.setBrush(QColor(0, 0, 0, 62));
  painter.drawEllipse(at, radius_px * 1.02F, radius_px * 0.9F);
  painter.restore();
}

void paint_natural_sprite(QPainter& painter,
                          NaturalSprite sprite,
                          const QPointF& centre,
                          float cell_px,
                          float scale,
                          unsigned seed) {
  if (sprite == NaturalSprite::None || cell_px <= 0.0F) {
    return;
  }
  scale = std::clamp(scale, 0.45F, 2.2F);
  const float vary = 0.86F + 0.28F * unit_random(seed, 1U);
  painter.setPen(Qt::NoPen);
  const QColor shadow(6, 12, 10, 78);

  switch (sprite) {
  case NaturalSprite::Pine: {
    const float r = std::max(1.55F * scale * vary * cell_px, 1.6F);
    painter.setBrush(shadow);
    painter.drawEllipse(centre + QPointF(r * 0.42F, r * 0.52F), r * 0.95F, r * 0.82F);
    if (r > 4.5F) {
      QPolygonF star;
      const float turn = unit_random(seed, 2U) * 6.283F;
      constexpr int k_points = 9;
      for (int index = 0; index < k_points * 2; ++index) {
        const float angle = turn + static_cast<float>(index) *
                                       std::numbers::pi_v<float> /
                                       static_cast<float>(k_points);
        const float reach = index % 2 == 0 ? r : r * 0.74F;
        star << (centre + QPointF(std::cos(angle) * reach, std::sin(angle) * reach));
      }
      painter.setBrush(QColor(26, 66, 44));
      painter.drawPolygon(star);
    } else {
      painter.setBrush(QColor(26, 66, 44));
      painter.drawEllipse(centre, r, r);
    }
    painter.setBrush(QColor(44, 102, 60));
    painter.drawEllipse(centre - QPointF(r * 0.12F, r * 0.14F), r * 0.68F, r * 0.68F);
    painter.setBrush(QColor(88, 146, 84));
    painter.drawEllipse(centre - QPointF(r * 0.24F, r * 0.28F), r * 0.34F, r * 0.34F);
    break;
  }
  case NaturalSprite::Olive: {
    const float r = std::max(1.5F * scale * vary * cell_px, 1.6F);
    painter.setBrush(shadow);
    painter.drawEllipse(centre + QPointF(r * 0.36F, r * 0.44F), r * 1.0F, r * 0.82F);
    const float turn = unit_random(seed, 2U) * 6.283F;
    painter.setBrush(QColor(74, 96, 62));
    for (int lobe = 0; lobe < 3; ++lobe) {
      const float angle = turn + static_cast<float>(lobe) * 2.094F;
      painter.drawEllipse(centre +
                              QPointF(std::cos(angle), std::sin(angle)) * (r * 0.42F),
                          r * 0.66F,
                          r * 0.66F);
    }
    painter.setBrush(QColor(112, 134, 86));
    for (int lobe = 0; lobe < 3; ++lobe) {
      const float angle = turn + static_cast<float>(lobe) * 2.094F;
      painter.drawEllipse(centre +
                              QPointF(std::cos(angle), std::sin(angle)) * (r * 0.38F) -
                              QPointF(r * 0.1F, r * 0.12F),
                          r * 0.46F,
                          r * 0.46F);
    }
    painter.setBrush(QColor(156, 170, 118));
    painter.drawEllipse(centre - QPointF(r * 0.26F, r * 0.3F), r * 0.26F, r * 0.26F);
    break;
  }
  case NaturalSprite::Cypress: {
    const float r = std::max(0.85F * scale * vary * cell_px, 1.3F);
    painter.setBrush(shadow);
    QPolygonF cast;
    cast << (centre + QPointF(-r * 0.5F, r * 0.3F))
         << (centre + QPointF(r * 0.5F, -r * 0.3F))
         << (centre + QPointF(r * 2.3F, r * 2.5F));
    painter.drawPolygon(cast);
    painter.setBrush(QColor(18, 46, 36));
    painter.drawEllipse(centre, r, r);
    painter.setBrush(QColor(34, 74, 52));
    painter.drawEllipse(centre - QPointF(r * 0.14F, r * 0.16F), r * 0.66F, r * 0.66F);
    painter.setBrush(QColor(64, 110, 74));
    painter.drawEllipse(centre - QPointF(r * 0.28F, r * 0.3F), r * 0.28F, r * 0.28F);
    break;
  }
  case NaturalSprite::Palm: {
    const float r = std::max(1.6F * scale * vary * cell_px, 1.8F);
    const float turn = unit_random(seed, 2U) * 6.283F;
    constexpr int k_fronds = 8;
    const auto fronds = [&](const QPointF& origin, const QColor& color, float width) {
      painter.setPen(QPen(color, std::max(width, 0.8F), Qt::SolidLine, Qt::RoundCap));
      for (int frond = 0; frond < k_fronds; ++frond) {
        const float angle = turn + static_cast<float>(frond) * 6.283F / k_fronds;
        const float reach = r * (0.82F + 0.3F * unit_random(seed, 20U + frond));
        painter.drawLine(origin,
                         origin + QPointF(std::cos(angle), std::sin(angle)) * reach);
      }
      painter.setPen(Qt::NoPen);
    };
    fronds(centre + QPointF(r * 0.34F, r * 0.42F), shadow, r * 0.3F);
    fronds(centre, QColor(44, 92, 50), r * 0.3F);
    fronds(centre - QPointF(r * 0.05F, r * 0.06F), QColor(96, 150, 78), r * 0.13F);
    painter.setBrush(QColor(104, 82, 54));
    painter.drawEllipse(centre, r * 0.16F, r * 0.16F);
    break;
  }
  case NaturalSprite::DeadTree: {
    const float r = std::max(1.0F * scale * vary * cell_px, 1.6F);
    const float turn = unit_random(seed, 2U) * 6.283F;
    const auto limbs = [&](const QPointF& origin, const QColor& color) {
      for (int limb = 0; limb < 5; ++limb) {
        const float angle = turn + static_cast<float>(limb) * 1.2566F +
                            (unit_random(seed, 30U + limb) - 0.5F) * 0.7F;
        const float reach = r * (0.55F + 0.45F * unit_random(seed, 40U + limb));
        painter.setPen(
            QPen(color, std::max(r * 0.14F, 0.8F), Qt::SolidLine, Qt::RoundCap));
        const QPointF tip = origin + QPointF(std::cos(angle), std::sin(angle)) * reach;
        painter.drawLine(origin, tip);
        painter.setPen(
            QPen(color, std::max(r * 0.07F, 0.6F), Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(origin + (tip - origin) * 0.6F,
                         tip + QPointF(std::cos(angle + 0.9F), std::sin(angle + 0.9F)) *
                                   (reach * 0.3F));
      }
      painter.setPen(Qt::NoPen);
    };
    limbs(centre + QPointF(r * 0.25F, r * 0.32F), QColor(6, 10, 8, 80));
    limbs(centre, QColor(96, 78, 62));
    painter.setBrush(QColor(128, 108, 88));
    painter.drawEllipse(centre, r * 0.18F, r * 0.18F);
    break;
  }
  case NaturalSprite::Boulder:
  case NaturalSprite::IronOre: {
    const float r = std::max(0.62F * scale * vary * cell_px, 1.3F);
    const bool ore = sprite == NaturalSprite::IronOre;
    painter.setBrush(shadow);
    painter.drawPolygon(
        blob(centre + QPointF(r * 0.34F, r * 0.42F), r * 1.02F, seed, 7, 0.14F));
    painter.setBrush(ore ? QColor(92, 66, 54) : QColor(104, 100, 96));
    painter.drawPolygon(blob(centre, r, seed, 7, 0.14F));
    painter.setBrush(ore ? QColor(136, 92, 66) : QColor(146, 142, 134));
    painter.drawPolygon(
        blob(centre - QPointF(r * 0.14F, r * 0.16F), r * 0.68F, seed + 5U, 6, 0.16F));
    painter.setBrush(ore ? QColor(186, 132, 88) : QColor(190, 186, 176));
    painter.drawPolygon(
        blob(centre - QPointF(r * 0.28F, r * 0.3F), r * 0.3F, seed + 9U, 5, 0.18F));
    if (ore && r > 3.0F) {
      painter.setBrush(QColor(44, 38, 40));
      for (unsigned speck = 0; speck < 3U; ++speck) {
        painter.drawEllipse(centre +
                                QPointF((unit_random(seed, 50U + speck) - 0.5F) * r,
                                        (unit_random(seed, 60U + speck) - 0.5F) * r),
                            r * 0.12F,
                            r * 0.12F);
      }
    }
    break;
  }
  case NaturalSprite::Plant: {
    const float r = std::max(0.36F * scale * vary * cell_px, 0.9F);
    const float turn = unit_random(seed, 2U) * 6.283F;
    painter.setBrush(QColor(8, 14, 10, 60));
    painter.drawEllipse(centre + QPointF(r * 0.3F, r * 0.36F), r, r * 0.8F);
    for (int leaf = 0; leaf < 4; ++leaf) {
      const float angle = turn + static_cast<float>(leaf) * 1.5708F;
      painter.setBrush(leaf % 2 == 0 ? QColor(70, 118, 60) : QColor(102, 150, 76));
      painter.drawEllipse(centre +
                              QPointF(std::cos(angle), std::sin(angle)) * (r * 0.45F),
                          r * 0.5F,
                          r * 0.5F);
    }
    break;
  }
  case NaturalSprite::None:
    break;
  }
}

void paint_cloud_shadows(QPainter& painter,
                         const QRectF& map_rect,
                         float cell_px,
                         double seconds,
                         float strength) {
  static const QImage k_clouds = [] {
    constexpr int k_size = 256;
    QImage image(k_size, k_size, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < k_size; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < k_size; ++x) {
        float total = 0.0F;
        float weight = 0.5F;
        float norm = 0.0F;
        int period = 4;
        for (int octave = 0; octave < 4; ++octave) {
          const float fx = static_cast<float>(x) / k_size * static_cast<float>(period);
          const float fy = static_cast<float>(y) / k_size * static_cast<float>(period);
          const int ix = static_cast<int>(std::floor(fx));
          const int iy = static_cast<int>(std::floor(fy));
          float tx = fx - static_cast<float>(ix);
          float ty = fy - static_cast<float>(iy);
          tx = tx * tx * (3.0F - 2.0F * tx);
          ty = ty * ty * (3.0F - 2.0F * ty);
          const auto corner = [&](int cx, int cy) {
            return hash_cell(
                cx % period, cy % period, 977U + static_cast<unsigned>(octave));
          };
          const float top = corner(ix, iy) + (corner(ix + 1, iy) - corner(ix, iy)) * tx;
          const float bottom =
              corner(ix, iy + 1) + (corner(ix + 1, iy + 1) - corner(ix, iy + 1)) * tx;
          total += (top + (bottom - top) * ty) * weight;
          norm += weight;
          weight *= 0.5F;
          period *= 2;
        }
        const float cover = smoothstep(0.54F, 0.74F, total / norm);
        const int alpha = static_cast<int>(cover * 255.0F);
        row[x] = qRgba(0, 0, 0, alpha);
      }
    }
    return image;
  }();

  if (strength <= 0.0F || map_rect.isEmpty()) {
    return;
  }
  constexpr float k_cells_per_tile = 150.0F;
  const float tile_px = std::max(k_cells_per_tile * cell_px, 64.0F);
  QBrush brush(k_clouds);
  QTransform transform;
  transform.translate(map_rect.left() + seconds * 0.9 * cell_px,
                      map_rect.top() + seconds * 0.5 * cell_px);
  transform.scale(tile_px / k_clouds.width(), tile_px / k_clouds.height());
  brush.setTransform(transform);
  painter.save();
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.setOpacity(strength);
  painter.fillRect(map_rect, brush);
  painter.restore();
}

void paint_world_surround(QPainter& painter,
                          const QRectF& canvas_rect,
                          const QRectF& map_rect) {
  painter.save();
  QRadialGradient table(canvas_rect.center(),
                        std::max(canvas_rect.width(), canvas_rect.height()) * 0.75);
  table.setColorAt(0.0, QColor(44, 37, 30));
  table.setColorAt(0.6, QColor(27, 23, 19));
  table.setColorAt(1.0, QColor(13, 11, 10));
  painter.fillRect(canvas_rect, table);

  painter.setPen(Qt::NoPen);
  for (int ring = 9; ring >= 1; --ring) {
    const double grow = ring * 2.4;
    painter.setBrush(QColor(0, 0, 0, 13));
    painter.drawRoundedRect(
        map_rect.adjusted(-grow + 3.0, -grow + 5.0, grow + 3.0, grow + 5.0),
        grow,
        grow);
  }
  painter.restore();
}

auto render_world_backdrop(const QByteArray& map_json,
                           const WorldBackdropOptions& options) -> WorldBackdrop {
  WorldBackdrop result;

  QTemporaryFile file;
  if (!file.open()) {
    result.error = QStringLiteral("cannot stage the map for the terrain builder");
    return result;
  }
  if (file.write(map_json) != map_json.size() || !file.flush()) {
    result.error = file.errorString();
    return result;
  }

  Game::Map::MapDefinition map;
  if (!Game::Map::MapLoader::load_from_json_file(file.fileName(), map, &result.error)) {
    return result;
  }
  if (!options.show_terrain) {
    map.terrain.clear();
    map.lakes.clear();
  }
  if (!options.show_paths) {
    map.rivers.clear();
    map.roads.clear();
    map.bridges.clear();
    map.fords.clear();
  }
  const int cells_w = map.grid.width;
  const int cells_h = map.grid.height;
  if (cells_w < 2 || cells_h < 2) {
    result.error = QStringLiteral("the map has no ground");
    return result;
  }

  Game::Session::SessionContext session;
  Game::Session::ScopedThreadSession const bound(session);
  session.terrain().initialize(map);
  const Game::Map::TerrainHeightMap* built = session.terrain().get_height_map();
  if (built == nullptr) {
    result.error = QStringLiteral("the terrain builder produced no ground");
    return result;
  }
  const Game::Map::TerrainHeightMap& height_map = *built;

  const auto& heights = height_map.get_height_data();
  const auto& types = height_map.getTerrainTypes();
  const auto& entrances = height_map.getHillEntrances();
  const auto cell_count =
      static_cast<std::size_t>(cells_w) * static_cast<std::size_t>(cells_h);
  if (heights.size() != cell_count || types.size() != cell_count) {
    result.error = QStringLiteral("the terrain builder returned an unexpected grid");
    return result;
  }

  MapFrame frame;
  frame.width = cells_w;
  frame.height = cells_h;
  frame.tile = std::max(map.grid.tile_size, 0.0001F);
  frame.grid_coords = map.coordSystem == Game::Map::CoordSystem::Grid;
  frame.ppc = std::clamp(static_cast<float>(options.max_dimension_px) /
                             static_cast<float>(std::max(cells_w, cells_h)),
                         options.min_pixels_per_cell,
                         options.max_pixels_per_cell);
  const int image_w = std::max(2, static_cast<int>(std::lround(cells_w * frame.ppc)));
  const int image_h = std::max(2, static_cast<int>(std::lround(cells_h * frame.ppc)));

  Field height_cells(cells_w, cells_h);
  Field shore(cells_w, cells_h);
  Field to_water(cells_w, cells_h, 1.0e6F);
  Field to_land(cells_w, cells_h, 1.0e6F);
  Field forest(cells_w, cells_h);
  Field mountain(cells_w, cells_h);
  Field hill(cells_w, cells_h);
  Field ramp(cells_w, cells_h);
  float highest = 0.01F;
  bool any_water = false;
  for (int z = 0; z < cells_h; ++z) {
    for (int x = 0; x < cells_w; ++x) {
      const auto index =
          static_cast<std::size_t>(z) * static_cast<std::size_t>(cells_w) +
          static_cast<std::size_t>(x);
      const auto type = types[index];
      const bool water =
          type == Game::Map::TerrainType::River || type == Game::Map::TerrainType::Lake;
      height_cells.ref(x, z) = heights[index];
      highest = std::max(highest, heights[index]);
      (water ? to_water : to_land).ref(x, z) = 0.0F;
      any_water = any_water || water;
      forest.ref(x, z) = type == Game::Map::TerrainType::Forest ? 1.0F : 0.0F;
      mountain.ref(x, z) = type == Game::Map::TerrainType::Mountain ? 1.0F : 0.0F;
      hill.ref(x, z) = type == Game::Map::TerrainType::Hill ? 1.0F : 0.0F;
      ramp.ref(x, z) = index < entrances.size() && entrances[index] ? 1.0F : 0.0F;
    }
  }
  soften(forest, 4);
  soften(mountain, 2);
  soften(hill, 2);
  soften(ramp, 1);
  if (any_water) {
    chamfer(to_water);
    chamfer(to_land);
  }
  for (int z = 0; z < cells_h; ++z) {
    for (int x = 0; x < cells_w; ++x) {
      if (!any_water) {
        shore.ref(x, z) = 99.0F;
      } else if (to_water.ref(x, z) <= 0.0F) {
        shore.ref(x, z) = -(std::min(to_land.ref(x, z), 12.0F) - 0.5F);
      } else {
        shore.ref(x, z) = std::min(to_water.ref(x, z), 12.0F) - 0.5F;
      }
    }
  }

  Field surface(image_w, image_h);
  for (int py = 0; py < image_h; ++py) {
    const float gz =
        static_cast<float>(cells_h) - (static_cast<float>(py) + 0.5F) / frame.ppc;
    for (int px = 0; px < image_w; ++px) {
      const float gx =
          static_cast<float>(cells_w) - (static_cast<float>(px) + 0.5F) / frame.ppc;
      surface.ref(px, py) = height_cells.sample_smooth(gx, gz);
    }
  }

  const auto& biome = map.biome;
  const std::uint32_t seed = biome.seed;
  const float saturation = std::clamp(biome.grass_saturation, 0.4F, 1.4F);
  const Rgb grass_a = rgb_of(biome.grass_primary);
  const Rgb grass_b = rgb_of(biome.grass_secondary);
  const Rgb grass_dry = rgb_of(biome.grass_dry);
  const Rgb soil = rgb_of(biome.soil_color);
  const Rgb rock_low = rgb_of(biome.rock_low);
  const Rgb rock_high = rgb_of(biome.rock_high);
  const Rgb snow = rgb_of(biome.snow_color);
  const float snow_cover = std::clamp(biome.snow_coverage, 0.0F, 1.0F);
  const float moisture = std::clamp(biome.moisture_level, 0.0F, 1.0F);
  const Rgb sand = mix(scaled(develop(soil), 1.3F), Rgb{0.74F, 0.67F, 0.50F}, 0.55F);
  const Rgb water_shallow{0.33F, 0.60F, 0.63F};
  const Rgb water_deep{0.09F, 0.27F, 0.42F};
  const Rgb foam{0.86F, 0.93F, 0.92F};

  const float light_len =
      std::sqrt(k_light_x * k_light_x + k_light_y * k_light_y + k_light_z * k_light_z);
  const float lx = k_light_x / light_len;
  const float ly = k_light_y / light_len;
  const float lz = k_light_z / light_len;
  const float planar = std::hypot(lx, ly);
  const float toward_x = lx / planar;
  const float toward_y = ly / planar;
  const float rise = lz / planar;
  const float world_per_px = frame.tile / frame.ppc;

  const bool night = map.time_of_day == Game::Map::TimeOfDay::Night;
  result.night = night ? 1.0F : 0.0F;

  QImage image(image_w, image_h, QImage::Format_RGB32);
  for (int py = 0; py < image_h; ++py) {
    auto* row = reinterpret_cast<QRgb*>(image.scanLine(py));
    const float gz =
        static_cast<float>(cells_h) - (static_cast<float>(py) + 0.5F) / frame.ppc;
    for (int px = 0; px < image_w; ++px) {
      const float gx =
          static_cast<float>(cells_w) - (static_cast<float>(px) + 0.5F) / frame.ppc;

      const float warp_x = (fbm(gx * 0.61F, gz * 0.61F, seed + 7U, 2) - 0.5F) * 0.7F;
      const float warp_z =
          (fbm(gx * 0.61F + 19.0F, gz * 0.61F - 7.0F, seed + 8U, 2) - 0.5F) * 0.7F;
      const float edge = shore.sample(gx + warp_x, gz + warp_z);

      const float macro = fbm(gx * 0.035F, gz * 0.035F, seed + 1U, 3);
      const float patch = fbm(gx * 0.13F, gz * 0.13F, seed + 2U, 3);
      const float grain = fbm(gx * 0.9F, gz * 0.9F, seed + 3U, 2);

      Rgb color;
      if (edge < 0.0F) {
        const float depth = -edge;
        color = mix(water_shallow, water_deep, smoothstep(0.0F, 3.4F, depth));
        const float ripple =
            fbm(gx * 1.4F + patch * 2.0F, gz * 1.4F, seed + 5U, 2) - 0.5F;
        color = scaled(color, 1.0F + ripple * 0.16F);
        color = mix(color, foam, (1.0F - smoothstep(0.0F, 0.42F, depth)) * 0.5F);
        const float ground = surface.at(px, py);
        float shade = 0.0F;
        for (int step = 1; step <= 6; ++step) {
          const float reach = static_cast<float>(step) * frame.ppc * 0.7F;
          const float over = surface.sample(static_cast<float>(px) + toward_x * reach,
                                            static_cast<float>(py) + toward_y * reach) -
                             ground - reach * world_per_px * rise;
          shade = std::max(shade, std::clamp(over / 0.8F, 0.0F, 1.0F));
        }
        color = scaled(color, 1.0F - 0.3F * shade);
      } else {
        Rgb ground = mix(grass_a, grass_b, smoothstep(0.34F, 0.68F, macro));
        ground = mix(ground,
                     grass_dry,
                     smoothstep(0.52F, 0.82F, patch) * (0.62F - 0.34F * moisture));
        ground = mix(
            ground,
            soil,
            smoothstep(0.66F, 0.9F, fbm(gx * 0.07F + 31.0F, gz * 0.07F, seed + 4U, 3)) *
                0.42F);
        ground = saturate(ground, saturation);
        ground = develop(ground);
        ground = scaled(ground, 0.92F + 0.16F * grain);

        const float sx =
            (surface.at(px + 1, py) - surface.at(px - 1, py)) / (2.0F * world_per_px);
        const float sy =
            (surface.at(px, py + 1) - surface.at(px, py - 1)) / (2.0F * world_per_px);
        const float slope = std::hypot(sx, sy);
        const float here = surface.at(px, py);

        const float hill_here = hill.sample(gx, gz);
        const float mountain_here = mountain.sample(gx, gz);
        const float forest_here = forest.sample(gx, gz);
        const float ramp_here = ramp.sample(gx, gz);

        ground =
            mix(ground, develop(saturate(grass_dry, saturation)), hill_here * 0.2F);
        const Rgb rock =
            develop(mix(rock_low, rock_high, smoothstep(0.3F, 0.75F, patch)));
        const float rocky = std::clamp(smoothstep(0.75F, 1.7F, slope) +
                                           mountain_here * (0.62F + 0.25F * patch),
                                       0.0F,
                                       1.0F);
        ground = mix(ground, scaled(rock, 0.66F + 0.16F * grain), rocky);
        const float litter =
            smoothstep(0.25F, 0.8F, forest_here + (patch - 0.5F) * 0.5F);
        ground = mix(ground,
                     mix(scaled(ground, 0.6F), develop(scaled(soil, 0.9F)), 0.3F),
                     litter * 0.5F);
        ground = mix(ground, sand, ramp_here * 0.3F);

        float snowy = 0.0F;
        if (snow_cover > 0.02F) {
          const float drift =
              std::clamp((macro * 0.55F + patch * 0.45F - 0.3F) / 0.4F, 0.0F, 1.0F);
          snowy =
              smoothstep(1.0F - snow_cover - 0.16F, 1.0F - snow_cover + 0.2F, drift);
          snowy =
              std::max(snowy, mountain_here * smoothstep(0.35F, 0.8F, here / highest));
          snowy *= 1.0F - smoothstep(0.9F, 1.9F, slope) * 0.75F;
        } else {
          snowy = mountain_here * smoothstep(0.62F, 0.95F, here / highest) *
                  (1.0F - smoothstep(0.9F, 1.9F, slope) * 0.6F) * 0.8F;
        }
        ground = mix(ground, snow, std::clamp(snowy, 0.0F, 1.0F));

        ground = mix(ground, sand, (1.0F - smoothstep(0.0F, 1.5F, edge)) * 0.72F);

        const float relief_x = sx * 0.62F;
        const float relief_y = sy * 0.62F;
        const float nl =
            1.0F / std::sqrt(relief_x * relief_x + relief_y * relief_y + 1.0F);
        const float lambert =
            std::max((-relief_x * lx - relief_y * ly + lz) * nl, 0.0F);
        float cast = 0.0F;
        for (int step = 1; step <= 10; ++step) {
          const float reach = static_cast<float>(step) * frame.ppc * 0.6F;
          const float over = surface.sample(static_cast<float>(px) + toward_x * reach,
                                            static_cast<float>(py) + toward_y * reach) -
                             here - reach * world_per_px * rise;
          cast = std::max(cast, std::clamp(over / 0.7F, 0.0F, 1.0F));
        }
        float light = std::clamp(0.56F + 0.74F * lambert, 0.5F, 1.22F);
        light *= 1.0F - 0.3F * cast;
        ground = scaled(ground, light);

        const float interval = highest > 9.0F ? 3.0F : 1.5F;
        if (here > 0.35F) {
          const float band = here / interval;
          const float near = std::abs(band - std::round(band)) * interval;
          const float width = std::max(slope * world_per_px * 0.9F, 0.02F);
          ground = scaled(ground,
                          1.0F - 0.09F * (1.0F - smoothstep(0.0F, width, near)) *
                                     smoothstep(0.04F, 0.16F, slope));
        }
        color = ground;
      }

      if (night) {
        color =
            mix(color, Rgb{color.r * 0.34F, color.g * 0.44F, color.b * 0.68F}, 0.62F);
      } else if (map.time_of_day == Game::Map::TimeOfDay::Afternoon) {
        color = {color.r * 1.04F, color.g * 0.99F, color.b * 0.9F};
      } else if (map.time_of_day == Game::Map::TimeOfDay::Morning) {
        color = {color.r * 1.01F, color.g * 0.99F, color.b * 0.97F};
      }
      row[px] = to_qcolor(color).rgb();
    }
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  paint_roads(painter, map, frame, soil);
  paint_bridges(painter, map, frame);

  const auto prop_key = [](const Game::Map::WorldProp& prop) {
    return (static_cast<std::uint64_t>(
                static_cast<std::int64_t>(std::lround(prop.x * 64.0F)) & 0xFFFFFF)
            << 32U) |
           (static_cast<std::uint64_t>(
                static_cast<std::int64_t>(std::lround(prop.z * 64.0F)) & 0xFFFFFF)
            << 8U) |
           static_cast<std::uint64_t>(prop.type);
  };
  std::vector<std::uint64_t> authored;
  authored.reserve(session.terrain().authored_world_props().size());
  for (const auto& prop : session.terrain().authored_world_props()) {
    authored.push_back(prop_key(prop));
  }
  std::sort(authored.begin(), authored.end());
  std::vector<Game::Map::WorldProp> props;
  props.reserve(session.terrain().world_props().size());
  for (const auto& prop : session.terrain().world_props()) {
    if (!std::binary_search(authored.begin(), authored.end(), prop_key(prop))) {
      props.push_back(prop);
    }
  }
  std::stable_sort(props.begin(), props.end(), [](const auto& a, const auto& b) {
    return a.z > b.z;
  });
  unsigned serial = 0U;
  for (const auto& prop : props) {
    const NaturalSprite sprite = sprite_of(prop.type);
    ++serial;
    if (sprite == NaturalSprite::None) {
      continue;
    }
    const float grid_x =
        frame.grid_coords ? prop.x : frame.grid_of_world(prop.x, cells_w);
    const float grid_z =
        frame.grid_coords ? prop.z : frame.grid_of_world(prop.z, cells_h);
    paint_natural_sprite(painter,
                         sprite,
                         frame.image_point(grid_x, grid_z),
                         frame.ppc,
                         prop.scale,
                         serial * 2654435761U + seed);
  }
  if (night) {
    painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    painter.fillRect(image.rect(), QColor(172, 186, 226));
  }
  painter.end();

  result.image = image;
  result.grid_width = cells_w;
  result.grid_height = cells_h;
  result.pixels_per_cell = frame.ppc;
  return result;
}

} // namespace MapEditor
