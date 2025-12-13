#include "humanoid/humanoid_math.h"
#include <algorithm>
#include <qvectornd.h>

namespace Render::GL {

auto elbow_bend_torso(const QVector3D &shoulder, const QVector3D &hand,
                      const QVector3D &outwardDir, float alongFrac,
                      float lateral_offset, float yBias,
                      float outwardSign) -> QVector3D {
  QVector3D dir = hand - shoulder;
  float const dist = std::max(dir.length(), 1e-5F);
  dir /= dist;

  QVector3D lateral = outwardDir - dir * QVector3D::dotProduct(outwardDir, dir);
  if (lateral.lengthSquared() < 1e-8F) {
    lateral = QVector3D::crossProduct(dir, QVector3D(0, 1, 0));
  }
  if (QVector3D::dotProduct(lateral, outwardDir) < 0.0F) {
    lateral = -lateral;
  }
  lateral.normalize();

  return shoulder + dir * (dist * alongFrac) +
         lateral * (lateral_offset * outwardSign) + QVector3D(0, yBias, 0);
}

} // namespace Render::GL
