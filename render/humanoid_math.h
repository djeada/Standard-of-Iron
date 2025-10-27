#pragma once

#include <QVector3D>
#include <cmath>
#include <cstdint>

namespace Render::GL {

inline auto hash01(uint32_t x) -> float {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return (x & 0x00FFFFFF) / float(0x01000000);
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
