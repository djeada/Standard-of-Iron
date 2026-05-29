#pragma once

#include <QVector3D>

namespace Render::Humanoid::PosePrimitives {

[[nodiscard]] auto solve_elbow_ik(const QVector3D& shoulder,
                                  const QVector3D& hand,
                                  const QVector3D& outward_dir,
                                  float along_frac,
                                  float lateral_offset,
                                  float y_bias,
                                  float outward_sign) -> QVector3D;

} // namespace Render::Humanoid::PosePrimitives
