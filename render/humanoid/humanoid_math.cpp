#include "humanoid/humanoid_math.h"
#include <algorithm>
#include <qvectornd.h>

namespace Render::GL {

auto elbow_bend_torso(const QVector3D &shoulder, const QVector3D &hand,
                      const QVector3D &outward_dir, float along_frac,
                      float lateral_offset, float y_bias,
                      float outward_sign) -> QVector3D {
  QVector3D dir = hand - shoulder;
  float const dist = std::max(dir.length(), 1e-5F);
  dir /= dist;

  QVector3D lateral =
      outward_dir - dir * QVector3D::dotProduct(outward_dir, dir);
  if (lateral.lengthSquared() < 1e-8F) {
    lateral = QVector3D::crossProduct(dir, QVector3D(0, 1, 0));
  }
  if (QVector3D::dotProduct(lateral, outward_dir) < 0.0F) {
    lateral = -lateral;
  }
  lateral.normalize();

  return shoulder + dir * (dist * along_frac) +
         lateral * (lateral_offset * outward_sign) + QVector3D(0, y_bias, 0);
}

} // namespace Render::GL
