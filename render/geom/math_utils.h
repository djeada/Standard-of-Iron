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

constexpr inline float lerp(float a, float b, float t) noexcept {
  return a * (1.0f - t) + b * t;
}

inline QVector3D lerp(const QVector3D &a, const QVector3D &b,
                      float t) noexcept {
  return a * (1.0f - t) + b * t;
}

constexpr inline float easeInOutCubic(float t) noexcept {
  const float clamped = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
  return clamped < 0.5f ? 4.0f * clamped * clamped * clamped
                        : 1.0f - std::pow(-2.0f * clamped + 2.0f, 3.0f) / 2.0f;
}

constexpr inline float smoothstep(float a, float b, float x) noexcept {
  const float t = (x - a) / (b - a);
  const float clamped = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
  return clamped * clamped * (3.0f - 2.0f * clamped);
}

inline QVector3D nlerp(const QVector3D &a, const QVector3D &b,
                       float t) noexcept {
  QVector3D v = a * (1.0f - t) + b * t;
  if (v.lengthSquared() > 1e-6f)
    v.normalize();
  return v;
}

} // namespace Render::Geom
