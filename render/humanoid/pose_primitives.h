#pragma once

#include <QVector3D>

#include "../side.h"

namespace Render::GL {
struct HumanoidPose;
}

namespace Render::Humanoid::PosePrimitives {

struct KneeIkParams {
  float upper_leg_len{0.0F};
  float lower_leg_len{0.0F};
  float knee_floor{0.0F};
  QVector3D bend_preference{0.0F, 0.0F, 1.0F};
  bool clamp_to_hip_height{false};
};

[[nodiscard]] auto solve_elbow_ik(const QVector3D& shoulder,
                                  const QVector3D& hand,
                                  const QVector3D& outward_dir,
                                  float along_frac,
                                  float lateral_offset,
                                  float y_bias,
                                  float outward_sign) -> QVector3D;

[[nodiscard]] auto solve_knee_ik(const QVector3D& hip,
                                 const QVector3D& foot,
                                 const KneeIkParams& params) -> QVector3D;

[[nodiscard]] auto
compute_right_axis(const Render::GL::HumanoidPose& pose) -> QVector3D;

[[nodiscard]] auto compute_outward_dir(const Render::GL::HumanoidPose& pose,
                                       Render::GL::Side side) -> QVector3D;

} // namespace Render::Humanoid::PosePrimitives
