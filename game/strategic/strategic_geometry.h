#pragma once

#include <QByteArray>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <map>
#include <optional>

#include "strategic_world.h"

namespace Game::Strategic {

struct ProvinceShape {
  QString id;
  QString name;
  QVector<QPointF> triangles;
  QVector<QPointF> border;
  QPointF label;
  double min_u = 1.0;
  double min_v = 1.0;
  double max_u = 0.0;
  double max_v = 0.0;
};

class RegionGeometry {
public:
  [[nodiscard]] static auto
  from_provinces_json(const QByteArray& json,
                      QString* error = nullptr) -> std::optional<RegionGeometry>;

  [[nodiscard]] auto empty() const -> bool { return m_shapes.empty(); }
  [[nodiscard]] auto shape(const QString& region) const -> const ProvinceShape*;
  [[nodiscard]] auto region_ids() const -> QStringList;
  [[nodiscard]] auto contains(const QString& region, QPointF uv) const -> bool;
  [[nodiscard]] auto region_at(QPointF uv) const -> QString;
  [[nodiscard]] auto distance_to(const QString& region, QPointF uv) const -> double;
  [[nodiscard]] auto border_gap(const QString& a, const QString& b) const -> double;
  [[nodiscard]] auto shapes() const -> const std::map<QString, ProvinceShape>& {
    return m_shapes;
  }

private:
  std::map<QString, ProvinceShape> m_shapes;
};

[[nodiscard]] auto point_in_polygon(const QVector<LonLat>& polygon,
                                    const LonLat& point) -> bool;

[[nodiscard]] auto uv_of(const Projection& projection, const LonLat& point) -> QPointF;

} // namespace Game::Strategic
