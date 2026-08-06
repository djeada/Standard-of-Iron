#pragma once

#include <QVector3D>

#include "animation/rig/side.h"

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

struct ArmIkParams {
  float upper_arm_len{0.0F};
  float fore_arm_len{0.0F};
  QVector3D bend_preference{1.0F, 0.0F, 0.0F};
};

inline constexpr float k_relaxed_arm_reach_fraction = 0.985F;
inline constexpr float k_braced_arm_reach_fraction = 0.96F;
inline constexpr float k_committed_arm_reach_fraction = 0.94F;
inline constexpr float k_seated_arm_reach_fraction = 0.75F;

[[nodiscard]] auto
arm_reach_limit(float upper_arm_len,
                float fore_arm_len,
                float fraction = k_relaxed_arm_reach_fraction) noexcept -> float;

[[nodiscard]] auto humanoid_arm_reach_limit(
    float height_scale = 1.0F,
    float fraction = k_relaxed_arm_reach_fraction) noexcept -> float;

[[nodiscard]] auto clamp_to_reach(const QVector3D& root,
                                  const QVector3D& target,
                                  float max_reach) -> QVector3D;

[[nodiscard]] auto solve_arm_ik(const QVector3D& shoulder,
                                const QVector3D& hand,
                                const ArmIkParams& params) -> QVector3D;

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
