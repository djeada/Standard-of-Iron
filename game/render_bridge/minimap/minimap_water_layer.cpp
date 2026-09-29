#include "minimap_water_layer.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QRectF>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "minimap_palette.h"

namespace Game::Map::Minimap {

namespace {

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

} // namespace

void paint_water_layer(QImage& image,
                       const MapDefinition& map_def,
                       const MinimapProjection& projection) {
  if (map_def.rivers.empty() && map_def.lakes.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

  for (const auto& lake : map_def.lakes) {
    const auto [cx, cy] = projection.to_pixel(lake.center.x(), lake.center.z());
    const float half_width =
        std::max(projection.to_pixel_size(lake.width) * 0.5F, 2.0F);
    const float half_depth =
        std::max(projection.to_pixel_size(lake.depth) * 0.5F, 2.0F);
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
    const auto [x1, y1] = projection.to_pixel(river.start.x(), river.start.z());
    const auto [x2, y2] = projection.to_pixel(river.end.x(), river.end.z());

    float pixel_width = projection.to_pixel_size(river.width);
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

} // namespace Game::Map::Minimap
