#pragma once

#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::Geom {

inline auto clamp01(float x) -> float {
  return std::max(0.0F, std::min(1.0F, x));
}

inline auto clamp_f(float x, float min_val, float max_val) -> float {
  return std::max(min_val, std::min(max_val, x));
}

inline auto clamp_vec_01(const QVector3D &c) -> QVector3D {
  return {clamp01(c.x()), clamp01(c.y()), clamp01(c.z())};
}

inline auto clamp_vec(const QVector3D &c, float min_val,
                      float max_val) -> QVector3D {
  return {clamp_f(c.x(), min_val, max_val), clamp_f(c.y(), min_val, max_val),
          clamp_f(c.z(), min_val, max_val)};
}

constexpr auto lerp(float a, float b, float t) noexcept -> float {
  return a * (1.0F - t) + b * t;
}

inline auto lerp(const QVector3D &a, const QVector3D &b,
                 float t) noexcept -> QVector3D {
  return a * (1.0F - t) + b * t;
}

constexpr auto ease_in_out_cubic(float t) noexcept -> float {
  const float clamped = (t < 0.0F) ? 0.0F : ((t > 1.0F) ? 1.0F : t);
  return clamped < 0.5F ? 4.0F * clamped * clamped * clamped
                        : 1.0F - std::pow(-2.0F * clamped + 2.0F, 3.0F) / 2.0F;
}

constexpr auto smoothstep(float a, float b, float x) noexcept -> float {
  const float t = (x - a) / (b - a);
  const float clamped = (t < 0.0F) ? 0.0F : ((t > 1.0F) ? 1.0F : t);
  return clamped * clamped * (3.0F - 2.0F * clamped);
}

inline auto nlerp(const QVector3D &a, const QVector3D &b,
                  float t) noexcept -> QVector3D {
  QVector3D v = a * (1.0F - t) + b * t;
  if (v.lengthSquared() > 1e-6F) {
    v.normalize();
  }
  return v;
}

} // namespace Render::Geom
