

#pragma once

#include "../creature/part_graph.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QVector3D>

namespace Render::Humanoid {

struct HumanoidBodyMetrics {
  QVector3D right_axis{1.0F, 0.0F, 0.0F};
  QVector3D up_axis{0.0F, 1.0F, 0.0F};
  QVector3D forward_axis{0.0F, 0.0F, 1.0F};

  float torso_r{0.0F};
  float torso_depth{0.0F};
  float y_top_cover{0.0F};
  float shoulder_half_span{0.0F};

  float upper_arm_r{0.0F};
  float fore_arm_r{0.0F};
  float joint_r{0.0F};
  float hand_r{0.0F};

  float thigh_r{0.0F};
  float shin_r{0.0F};
  float leg_joint_r{0.0F};
  float foot_radius{0.0F};
};

void compute_humanoid_body_metrics(const Render::GL::HumanoidPose &pose,
                                   const QVector3D &proportion_scaling,
                                   float torso_scale,
                                   HumanoidBodyMetrics &out) noexcept;

void compute_humanoid_head_frame(Render::GL::HumanoidPose &pose,
                                 const HumanoidBodyMetrics &metrics) noexcept;

void compute_humanoid_body_frames(Render::GL::HumanoidPose &pose,
                                  const HumanoidBodyMetrics &metrics) noexcept;

} // namespace Render::Humanoid
