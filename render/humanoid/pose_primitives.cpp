#include "pose_primitives.h"

#include "humanoid_math.h"

namespace Render::Humanoid::PosePrimitives {

auto solve_elbow_ik(const QVector3D& shoulder,
                    const QVector3D& hand,
                    const QVector3D& outward_dir,
                    float along_frac,
                    float lateral_offset,
                    float y_bias,
                    float outward_sign) -> QVector3D {
  return Render::GL::elbow_bend_torso(
      shoulder, hand, outward_dir, along_frac, lateral_offset, y_bias, outward_sign);
}

} // namespace Render::Humanoid::PosePrimitives
