#pragma once

#include <QVector3D>

#include <cmath>
#include <cstdint>

#include "render/gl/render_constants.h"
#include "render/math/hash.h"

namespace Render::GL {

inline auto rot_y(const QVector3D& v, float angle_rad) -> QVector3D {
  const float c = std::cos(angle_rad);
  const float s = std::sin(angle_rad);
  return {c * v.x() + s * v.z(), v.y(), -s * v.x() + c * v.z()};
}

inline auto right_of(const QVector3D& fwd) -> QVector3D {
  const QVector3D UP(0.0F, 1.0F, 0.0F);
  return QVector3D::crossProduct(UP, fwd).normalized();
}

auto elbow_bend_torso(const QVector3D& shoulder,
                      const QVector3D& hand,
                      const QVector3D& outward_dir,
                      float along_frac,
                      float lateral_offset,
                      float y_bias,
                      float outward_sign) -> QVector3D;

} // namespace Render::GL
