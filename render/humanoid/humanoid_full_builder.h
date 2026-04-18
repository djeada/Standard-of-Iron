// Stage 15.5d — humanoid Full-LOD body-frame scaffolding.
//
// `compute_humanoid_body_metrics` / `compute_humanoid_head_frame` /
// `compute_humanoid_body_frames` together populate the scalar radii and
// per-bone `AttachmentFrame`s that armor, helmet and weapon attachments
// read out of `pose.body_frames`. These are pure CPU computations with
// no draw submission — geometry emission for the Full LOD goes through
// `submit_humanoid_lod` (see humanoid_spec.h).

#pragma once

#include "../creature/part_graph.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QVector3D>

namespace Render::Humanoid {

// Derived body metrics — pure function of (pose, proportion_scaling,
// torso_scale). Stable across the whole render for one entity-frame.
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

// Compute the derived metrics. No side effects.
void compute_humanoid_body_metrics(const Render::GL::HumanoidPose &pose,
                                   const QVector3D &proportion_scaling,
                                   float torso_scale,
                                   HumanoidBodyMetrics &out) noexcept;

// Populate `pose.head_frame` (origin/right/up/forward/radius) from the
// neck/head anchor points and the body basis. Extracted from the
// legacy `draw_common_body` so the Full LOD body path can build the
// frame without duplicating head geometry.
void compute_humanoid_head_frame(Render::GL::HumanoidPose &pose,
                                 const HumanoidBodyMetrics &metrics) noexcept;

// Populate `pose.body_frames` from pose + metrics. This is the contract
// downstream equipment renderers rely on (sword/shield/helmet/armor/
// shoulder cover/greaves/etc reach into body_frames). Callers must have
// already invoked `compute_humanoid_head_frame` because body_frames.head
// is mirrored from `pose.head_frame`.
void compute_humanoid_body_frames(Render::GL::HumanoidPose &pose,
                                  const HumanoidBodyMetrics &metrics) noexcept;

} // namespace Render::Humanoid
