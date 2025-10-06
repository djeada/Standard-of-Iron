#include "transforms.h"
#include <algorithm>
#include <cmath>

namespace Render::Geom {

QMatrix4x4 cylinderBetween(const QVector3D &a, const QVector3D &b,
                           float radius) {
  QVector3D mid = (a + b) * 0.5f;
  QVector3D dir = b - a;
  float len = dir.length();

  QMatrix4x4 M;
  M.translate(mid);

  if (len > 1e-6f) {
    QVector3D yAxis(0, 1, 0);
    QVector3D d = dir / len;
    float dot = std::clamp(QVector3D::dotProduct(yAxis, d), -1.0f, 1.0f);
    float angleDeg = std::acos(dot) * 57.2957795131f;
    QVector3D axis = QVector3D::crossProduct(yAxis, d);
    if (axis.lengthSquared() < 1e-6f) {

      if (dot < 0.0f) {
        M.rotate(180.0f, 1.0f, 0.0f, 0.0f);
      }
    } else {
      axis.normalize();
      M.rotate(angleDeg, axis);
    }
    M.scale(radius, len, radius);
  } else {
    M.scale(radius, 1.0f, radius);
  }
  return M;
}

QMatrix4x4 sphereAt(const QVector3D &pos, float radius) {
  QMatrix4x4 M;
  M.translate(pos);
  M.scale(radius, radius, radius);
  return M;
}

QMatrix4x4 coneFromTo(const QVector3D &baseCenter, const QVector3D &apex,
                      float baseRadius) {

  return cylinderBetween(baseCenter, apex, baseRadius);
}

} // namespace Render::Geom
