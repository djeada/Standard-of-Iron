#pragma once

#include <QVector3D>
#include <cmath>
#include <cstdint>

namespace Render::GL {

inline float hash01(uint32_t x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return (x & 0x00FFFFFF) / float(0x01000000);
}

inline QVector3D rotY(const QVector3D &v, float angleRad) {
  const float c = std::cos(angleRad);
  const float s = std::sin(angleRad);
  return QVector3D(c * v.x() + s * v.z(), v.y(), -s * v.x() + c * v.z());
}

inline QVector3D rightOf(const QVector3D &fwd) {
  const QVector3D UP(0.0f, 1.0f, 0.0f);
  return QVector3D::crossProduct(UP, fwd).normalized();
}

QVector3D elbowBendTorso(const QVector3D &shoulder, const QVector3D &hand,
                         const QVector3D &outwardDir, float alongFrac,
                         float lateralOffset, float yBias, float outwardSign);

}
