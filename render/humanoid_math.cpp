#include "humanoid_math.h"

namespace Render::GL {

QVector3D elbowBendTorso(const QVector3D &shoulder, const QVector3D &hand,
                         const QVector3D &outwardDir, float alongFrac,
                         float lateralOffset, float yBias, float outwardSign) {
  QVector3D dir = hand - shoulder;
  float dist = std::max(dir.length(), 1e-5f);
  dir /= dist;

  QVector3D lateral = outwardDir - dir * QVector3D::dotProduct(outwardDir, dir);
  if (lateral.lengthSquared() < 1e-8f) {
    lateral = QVector3D::crossProduct(dir, QVector3D(0, 1, 0));
  }
  if (QVector3D::dotProduct(lateral, outwardDir) < 0.0f)
    lateral = -lateral;
  lateral.normalize();

  return shoulder + dir * (dist * alongFrac) +
         lateral * (lateralOffset * outwardSign) + QVector3D(0, yBias, 0);
}

} // namespace Render::GL
