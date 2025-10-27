#pragma once

#include "gl/render_constants.h"
#include <QVector3D>
#include <cmath>
#include <cstdint>

namespace Render::GL {

inline auto hash01(uint32_t x) -> float {
  x ^= x << HashXorShift::Shift13;
  x ^= x >> HashXorShift::Shift17;
  x ^= x << HashXorShift::Shift5;
  return (x & BitShift::Mask24Bit) / float(BitShift::Mask24BitHex);
}

inline auto rotY(const QVector3D &v, float angle_rad) -> QVector3D {
  const float c = std::cos(angle_rad);
  const float s = std::sin(angle_rad);
  return {c * v.x() + s * v.z(), v.y(), -s * v.x() + c * v.z()};
}

inline auto rightOf(const QVector3D &fwd) -> QVector3D {
  const QVector3D UP(0.0F, 1.0F, 0.0F);
  return QVector3D::crossProduct(UP, fwd).normalized();
}

auto elbowBendTorso(const QVector3D &shoulder, const QVector3D &hand,
                    const QVector3D &outwardDir, float alongFrac,
                    float lateral_offset, float yBias,
                    float outwardSign) -> QVector3D;

} // namespace Render::GL
