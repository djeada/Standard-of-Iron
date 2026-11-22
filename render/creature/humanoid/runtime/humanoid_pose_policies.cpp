#include "render/creature/humanoid/runtime/humanoid_pose_policies.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid/runtime/pose_controller.h"

namespace Render::GL {
namespace {

auto channel_pose_weight(const Render::GL::HumanoidAnimationContext& anim) -> float {
  const auto& inputs = anim.inputs;
  if (inputs.is_dying || inputs.is_dead || inputs.is_attacking ||
      inputs.is_hit_reacting || inputs.is_routing) {
    return 0.0F;
  }
  if (inputs.is_healing || inputs.is_casting) {
    return 1.0F;
  }
  switch (inputs.movement_state) {
  case Render::Creature::MovementAnimationState::Idle:
    return 1.0F;
  case Render::Creature::MovementAnimationState::Walk:
    return 0.55F;
  default:
    return 0.22F;
  }
}

void apply_healer_channel_pose(const Render::GL::HumanoidAnimationContext& anim,
                               HumanoidPose& io_pose) {
  Render::Humanoid::apply_skeleton_proportion_pose(io_pose);
  float const weight = channel_pose_weight(anim);
  if (weight <= 0.0F) {
    return;
  }

  QVector3D const forward = anim.heading_forward();
  QVector3D const right = anim.heading_right();
  QVector3D const up = anim.heading_up();
  float const breathe = 0.014F * std::sin(anim.inputs.time * 1.6F);

  HumanoidPoseController controller(io_pose, anim);
  controller.tilt_torso(0.0F, -0.07F * weight);

  io_pose.hand_l +=
      (forward * 0.30F + up * (0.20F + breathe) + right * 0.045F) * weight;
  io_pose.hand_r +=
      (forward * 0.30F + up * (0.20F - breathe) - right * 0.045F) * weight;
  io_pose.elbow_l += (forward * 0.09F + up * 0.04F - right * 0.10F) * weight;
  io_pose.elbow_r += (forward * 0.09F + up * 0.04F + right * 0.10F) * weight;
  io_pose.head_pos += forward * (0.012F * weight);
}

void apply_healer_staff_pose(const Render::GL::HumanoidAnimationContext& anim,
                             HumanoidPose& io_pose) {
  Render::Humanoid::apply_skeleton_proportion_pose(io_pose);

  QVector3D const forward = anim.heading_forward();
  QVector3D const right = anim.heading_right();
  QVector3D const up = anim.heading_up();

  io_pose.hand_r += forward * 0.20F + right * 0.13F - up * 0.02F;
  io_pose.elbow_r += forward * 0.05F + right * 0.11F - up * 0.04F;

  float const weight = channel_pose_weight(anim);
  if (weight <= 0.0F) {
    return;
  }
  float const breathe = 0.016F * std::sin(anim.inputs.time * 1.4F);

  HumanoidPoseController controller(io_pose, anim);
  controller.tilt_torso(0.0F, -0.06F * weight);
  io_pose.hand_l += (forward * 0.34F + up * (0.26F + breathe) + right * 0.02F) * weight;
  io_pose.elbow_l += (forward * 0.10F + up * 0.06F - right * 0.09F) * weight;
  io_pose.head_pos += forward * (0.010F * weight);
}

} // namespace

namespace Carthage {
namespace {

void apply_grave_priest_cast_pose(const Render::GL::HumanoidAnimationContext& anim,
                                  HumanoidPose& io_pose) {
  Render::Humanoid::apply_skeleton_proportion_pose(io_pose);

  if (!anim.inputs.is_casting || anim.inputs.cast_kind != CastVisualKind::Fireball) {
    return;
  }

  float const phase = std::clamp(anim.attack_phase, 0.0F, 1.0F);
  auto ease = [](float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };
  float const gather = ease(phase / 0.30F);
  float const release = ease((phase - 0.30F) / 0.22F);
  float const recover = 1.0F - ease((phase - 0.60F) / 0.40F);
  float const intensity = gather * recover;
  float const reach = release * recover;
  if (intensity <= 0.0F) {
    return;
  }

  HumanoidPoseController controller(io_pose, anim);
  controller.tilt_torso(-0.13F * intensity, -0.18F * intensity);

  QVector3D const forward = anim.heading_forward();
  QVector3D const right = anim.heading_right();
  QVector3D const up = anim.heading_up();

  io_pose.hand_r += forward * (0.12F * intensity + 0.27F * reach) +
                    up * (0.21F * intensity) - right * (0.09F * intensity);
  io_pose.elbow_r += forward * (0.05F * intensity + 0.07F * reach) +
                     up * (0.12F * intensity) + right * (0.05F * intensity);
  io_pose.hand_l += forward * (0.12F * intensity + 0.10F * reach) +
                    up * (0.18F * intensity) + right * (0.10F * intensity);
  io_pose.elbow_l += forward * (0.01F * intensity + 0.04F * reach) +
                     up * (0.09F * intensity) - right * (0.07F * intensity);
  io_pose.head_pos += up * (0.015F * intensity) + forward * (0.025F * intensity);
}

} // namespace
} // namespace Carthage
} // namespace Render::GL

namespace Render::Entity {

void apply_humanoid_pose_policy(Render::Humanoid::HumanoidPosePolicy policy,
                                const HumanoidPosePolicyInputs& inputs,
                                Render::GL::HumanoidPose& io_pose) {
  using Render::Humanoid::HumanoidPosePolicy;

  switch (policy) {
  case HumanoidPosePolicy::None:
    return;
  case HumanoidPosePolicy::SkeletonProportions:
    Render::Humanoid::apply_skeleton_proportion_pose(io_pose);
    return;
  case HumanoidPosePolicy::HealerChannel:
    if (inputs.animation != nullptr) {
      Render::GL::apply_healer_channel_pose(*inputs.animation, io_pose);
    }
    return;
  case HumanoidPosePolicy::HealerStaff:
    if (inputs.animation != nullptr) {
      Render::GL::apply_healer_staff_pose(*inputs.animation, io_pose);
    }
    return;
  case HumanoidPosePolicy::GravePriestCast:
    if (inputs.animation != nullptr) {
      Render::GL::Carthage::apply_grave_priest_cast_pose(*inputs.animation, io_pose);
    }
    return;
  }
}

} // namespace Render::Entity
