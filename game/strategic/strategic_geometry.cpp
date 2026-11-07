#include "strategic_geometry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Strategic {

namespace {

auto read_point(const QJsonValue& value) -> std::optional<QPointF> {
  const QJsonArray pair = value.toArray();
  if (pair.size() < 2) {
    return std::nullopt;
  }
  return QPointF(pair.at(0).toDouble(), pair.at(1).toDouble());
}

auto cross(QPointF o, QPointF a, QPointF b) -> double {
  return ((a.x() - o.x()) * (b.y() - o.y())) - ((a.y() - o.y()) * (b.x() - o.x()));
}

auto in_triangle(QPointF p, QPointF a, QPointF b, QPointF c) -> bool {
  const double d1 = cross(a, b, p);
  const double d2 = cross(b, c, p);
  const double d3 = cross(c, a, p);
  const bool has_neg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
  const bool has_pos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);
  return !(has_neg && has_pos);
}

auto segment_distance(QPointF p, QPointF a, QPointF b) -> double {
  const double dx = b.x() - a.x();
  const double dy = b.y() - a.y();
  const double len2 = (dx * dx) + (dy * dy);
  double t = 0.0;
  if (len2 > 0.0) {
    t = std::clamp((((p.x() - a.x()) * dx) + ((p.y() - a.y()) * dy)) / len2, 0.0, 1.0);
  }
  const double qx = a.x() + (t * dx) - p.x();
  const double qy = a.y() + (t * dy) - p.y();
  return std::sqrt((qx * qx) + (qy * qy));
}

} // namespace

auto RegionGeometry::from_provinces_json(const QByteArray& json, QString* error)
    -> std::optional<RegionGeometry> {
  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(json, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    if (error != nullptr) {
      *error = parse_error.errorString();
    }
    return std::nullopt;
  }
  const QJsonObject root = document.object();
  const QJsonArray provinces = root.value(QStringLiteral("provinces")).toArray();
  const QJsonArray borders = root.value(QStringLiteral("borders")).toArray();
  RegionGeometry geometry;
  for (int i = 0; i < provinces.size(); ++i) {
    const QJsonObject province = provinces.at(i).toObject();
    ProvinceShape shape;
    shape.id = province.value(QStringLiteral("id")).toString();
    shape.name = province.value(QStringLiteral("name")).toString();
    if (const auto label = read_point(province.value(QStringLiteral("label_uv")))) {
      shape.label = *label;
    }
    for (const auto value : province.value(QStringLiteral("triangles")).toArray()) {
      if (const auto point = read_point(value)) {
        shape.triangles.append(*point);
        shape.min_u = std::min(shape.min_u, point->x());
        shape.min_v = std::min(shape.min_v, point->y());
        shape.max_u = std::max(shape.max_u, point->x());
        shape.max_v = std::max(shape.max_v, point->y());
      }
    }
    shape.triangles.resize(shape.triangles.size() - (shape.triangles.size() % 3));
    if (i < borders.size()) {
      for (const auto value : borders.at(i).toArray()) {
        if (const auto point = read_point(value)) {
          shape.border.append(*point);
        }
      }
    }
    if (!shape.id.isEmpty()) {
      geometry.m_shapes.emplace(shape.id, std::move(shape));
    }
  }
  if (geometry.m_shapes.empty()) {
    if (error != nullptr) {
      *error = QStringLiteral("no provinces in geometry file");
    }
    return std::nullopt;
  }
  return geometry;
}

auto RegionGeometry::shape(const QString& region) const -> const ProvinceShape* {
  const auto it = m_shapes.find(region);
  return it == m_shapes.end() ? nullptr : &it->second;
}

auto RegionGeometry::region_ids() const -> QStringList {
  QStringList out;
  for (const auto& [id, shape] : m_shapes) {
    out.append(id);
  }
  return out;
}

auto RegionGeometry::contains(const QString& region, QPointF uv) const -> bool {
  const auto* entry = shape(region);
  if (entry == nullptr) {
    return false;
  }
  if (uv.x() < entry->min_u || uv.x() > entry->max_u || uv.y() < entry->min_v ||
      uv.y() > entry->max_v) {
    return false;
  }
  const auto& tris = entry->triangles;
  for (int i = 0; i + 2 < tris.size(); i += 3) {
    if (in_triangle(uv, tris[i], tris[i + 1], tris[i + 2])) {
      return true;
    }
  }
  return false;
}

auto RegionGeometry::region_at(QPointF uv) const -> QString {
  for (const auto& [id, entry] : m_shapes) {
    if (contains(id, uv)) {
      return id;
    }
  }
  return {};
}

auto RegionGeometry::distance_to(const QString& region, QPointF uv) const -> double {
  if (contains(region, uv)) {
    return 0.0;
  }
  const auto* entry = shape(region);
  if (entry == nullptr) {
    return std::numeric_limits<double>::infinity();
  }
  double best = std::numeric_limits<double>::infinity();
  const auto& tris = entry->triangles;
  for (int i = 0; i + 2 < tris.size(); i += 3) {
    best = std::min({best,
                     segment_distance(uv, tris[i], tris[i + 1]),
                     segment_distance(uv, tris[i + 1], tris[i + 2]),
                     segment_distance(uv, tris[i + 2], tris[i])});
  }
  return best;
}

auto RegionGeometry::border_gap(const QString& a, const QString& b) const -> double {
  const auto* first = shape(a);
  const auto* second = shape(b);
  if (first == nullptr || second == nullptr || first->border.isEmpty() ||
      second->border.isEmpty()) {
    return std::numeric_limits<double>::infinity();
  }
  if (first->max_u + 0.05 < second->min_u || second->max_u + 0.05 < first->min_u ||
      first->max_v + 0.05 < second->min_v || second->max_v + 0.05 < first->min_v) {
    return std::numeric_limits<double>::infinity();
  }
  double best = std::numeric_limits<double>::infinity();
  const int stride_a = std::max<int>(1, static_cast<int>(first->border.size()) / 400);
  for (int i = 0; i < first->border.size(); i += stride_a) {
    const QPointF p = first->border[i];
    for (int j = 1; j < second->border.size(); ++j) {
      best =
          std::min(best, segment_distance(p, second->border[j - 1], second->border[j]));
    }
  }
  return best;
}

auto point_in_polygon(const QVector<LonLat>& polygon, const LonLat& point) -> bool {
  bool inside = false;
  const int n = static_cast<int>(polygon.size());
  for (int i = 0, j = n - 1; i < n; j = i++) {
    const LonLat& a = polygon[i];
    const LonLat& b = polygon[j];
    const bool crosses = (a.lat > point.lat) != (b.lat > point.lat);
    if (crosses) {
      const double lon_at =
          ((b.lon - a.lon) * (point.lat - a.lat) / (b.lat - a.lat)) + a.lon;
      if (point.lon < lon_at) {
        inside = !inside;
      }
    }
  }
  return inside;
}

auto uv_of(const Projection& projection, const LonLat& point) -> QPointF {
  return {projection.to_uv_u(point), projection.to_uv_v(point)};
}

} // namespace Game::Strategic
