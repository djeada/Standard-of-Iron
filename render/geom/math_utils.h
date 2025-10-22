#pragma once

#include <QVector3D>
#include <algorithm>
#include <cmath>

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

// Interpolation and easing functions

inline float lerp(float a, float b, float t) {
  return a * (1.0f - t) + b * t;
}

inline QVector3D lerp(const QVector3D &a, const QVector3D &b, float t) {
  return a * (1.0f - t) + b * t;
}

inline float easeInOutCubic(float t) {
  t = clamp01(t);
  return t < 0.5f ? 4.0f * t * t * t
                  : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

inline float smoothstep(float a, float b, float x) {
  x = clamp01((x - a) / (b - a));
  return x * x * (3.0f - 2.0f * x);
}

inline QVector3D nlerp(const QVector3D &a, const QVector3D &b, float t) {
  QVector3D v = a * (1.0f - t) + b * t;
  if (v.lengthSquared() > 1e-6f)
    v.normalize();
  return v;
}

} // namespace Render::Geom
