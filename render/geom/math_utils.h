#pragma once

#include <QVector3D>
#include <algorithm>

namespace Render::Geom {

inline float clamp01(float x) { return std::max(0.0f, std::min(1.0f, x)); }

inline float clampf(float x, float minVal, float maxVal) {
  return std::max(minVal, std::min(maxVal, x));
}

inline QVector3D clampVec01(const QVector3D &c) {
  return QVector3D(clamp01(c.x()), clamp01(c.y()), clamp01(c.z()));
}

inline QVector3D clampVec(const QVector3D &c, float minVal, float maxVal) {
  return QVector3D(clampf(c.x(), minVal, maxVal), clampf(c.y(), minVal, maxVal),
                   clampf(c.z(), minVal, maxVal));
}

} // namespace Render::Geom
