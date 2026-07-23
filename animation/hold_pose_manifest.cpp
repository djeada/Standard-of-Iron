#include "hold_pose_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation {

auto resolve_humanoid_held_pose(const HumanoidHeldPoseInputs& inputs) noexcept
    -> HumanoidHeldPoseSample {
  float const shoulder_y = inputs.shoulder_y;
  float const hold_cycle =
      std::sin(inputs.sample_time * (2.0F * std::numbers::pi_v<float>) / 1.8F);
  HumanoidHeldPoseSample sample{};

  switch (inputs.kind) {
  case HumanoidHeldPoseKind::SpearIdle:
    sample.right_hand = {0.34F, shoulder_y - 0.02F, 0.30F};
    sample.use_offhand_spear_grip = true;
    sample.offhand_spear_direction = {0.0493264F, 0.542590F, 0.838548F};
    sample.offhand_along_offset = 0.46F;
    sample.offhand_y_drop = -0.03F;
    sample.offhand_lateral_offset = -0.08F;
    sample.clamp_left_hand_x_min = true;
    sample.left_hand_x_min = 0.10F;
    sample.clamp_left_hand_y_max = true;
    sample.left_hand_y_max = shoulder_y + 0.12F;
    sample.shoulder_r_x_delta = 0.025F;
    sample.shoulder_l_x_delta = 0.015F;
    sample.shoulder_r_z_delta = 0.025F;
    sample.shoulder_l_z_delta = 0.020F;
    break;
  case HumanoidHeldPoseKind::SpearBrace:
    sample.right_hand = {
        0.30F, shoulder_y - 0.10F - 0.006F * hold_cycle, 0.58F + 0.014F * hold_cycle};
    sample.use_offhand_spear_grip = true;
    sample.offhand_spear_direction = {0.0502367F, 0.401893F, 0.914307F};
    sample.offhand_along_offset = -0.24F;
    sample.offhand_y_drop = 0.045F;
    sample.offhand_lateral_offset = -0.04F;
    sample.left_hand_z_delta = 0.03F;
    sample.shoulder_r_y_delta = -0.05F;
    sample.shoulder_l_y_delta = -0.03F;
    sample.shoulder_r_z_delta = 0.08F;
    sample.shoulder_l_z_delta = 0.06F;
    sample.neck_z_delta = 0.07F + 0.006F * hold_cycle;
    sample.head_z_delta = 0.06F;
    sample.head_y_delta = -0.01F;
    break;
  case HumanoidHeldPoseKind::BowReady:
    sample.right_hand = {
        0.06F, shoulder_y + 0.08F + 0.010F * hold_cycle, 0.70F - 0.008F * hold_cycle};
    sample.left_hand = {
        0.05F, shoulder_y + 0.04F + 0.006F * hold_cycle, 0.34F + 0.005F * hold_cycle};
    sample.shoulder_r_z_delta = 0.16F;
    sample.shoulder_l_y_delta = 0.02F;
    sample.shoulder_r_y_delta = -0.04F;
    sample.shoulder_l_z_delta = 0.01F;
    sample.neck_z_delta = 0.03F;
    sample.head_z_delta = 0.02F;
    sample.head_y_delta = -0.01F + 0.004F * hold_cycle;
    break;
  case HumanoidHeldPoseKind::SwordShieldCarry: {
    float const moving_mix = inputs.moving ? 1.0F : 0.0F;
    sample.right_hand = {0.34F + moving_mix * 0.03F,
                         shoulder_y - 0.06F - moving_mix * 0.05F,
                         0.44F + moving_mix * 0.06F};
    sample.left_hand = {-0.32F - moving_mix * 0.03F,
                        shoulder_y - 0.02F - moving_mix * 0.03F,
                        0.28F + moving_mix * 0.05F};
    break;
  }
  }

  return sample;
}

auto resolve_humanoid_guard_stance_pose(
    const HumanoidGuardStanceInputs& inputs) noexcept -> HumanoidGuardStanceSample {
  float const amount = std::clamp(inputs.amount, 0.0F, 1.0F);
  if (amount <= 1.0e-4F) {
    return {};
  }

  float const shoulder_y = inputs.shoulder_y;
  HumanoidGuardStanceSample sample{};
  sample.active = true;
  sample.blend_amount = amount;
  sample.right_hand = {0.20F, shoulder_y - 0.20F, 0.24F};
  sample.left_hand = {-0.12F, shoulder_y + 0.10F, 0.54F};
  sample.shoulder_l_delta = {0.0F, 0.04F * amount, 0.14F * amount};
  sample.shoulder_r_delta = {0.0F, -0.03F * amount, 0.06F * amount};
  sample.neck_delta = {0.0F, -0.01F * amount, 0.09F * amount};
  sample.head_delta = {0.0F, -0.01F * amount, 0.07F * amount};

  switch (inputs.pose) {
  case ShieldFormationPose::RomanTop:
    sample.right_hand = {0.14F, shoulder_y - 0.22F, 0.12F};
    sample.left_hand = {-0.04F, shoulder_y + 0.34F, 0.20F};
    sample.shoulder_l_delta = {0.0F, 0.08F * amount, 0.20F * amount};
    sample.shoulder_r_delta = {0.0F, -0.06F * amount, 0.03F * amount};
    sample.neck_delta = {0.0F, -0.04F * amount, 0.12F * amount};
    sample.head_delta = {0.0F, -0.05F * amount, 0.09F * amount};
    break;
  case ShieldFormationPose::RomanFront:
    sample.right_hand = {0.20F, shoulder_y - 0.20F, 0.20F};
    sample.left_hand = {-0.10F, shoulder_y + 0.14F, 0.56F};
    sample.shoulder_l_delta = {0.0F, 0.06F * amount, 0.17F * amount};
    sample.shoulder_r_delta = {0.0F, -0.05F * amount, 0.05F * amount};
    sample.neck_delta = {0.0F, -0.02F * amount, 0.11F * amount};
    sample.head_delta = {0.0F, -0.03F * amount, 0.08F * amount};
    break;
  case ShieldFormationPose::CarthageFront:
    sample.right_hand = {0.20F, shoulder_y - 0.08F, 0.26F};
    sample.left_hand = {-0.14F, shoulder_y + 0.26F, 0.34F};
    sample.shoulder_l_delta = {0.0F, 0.06F * amount, 0.18F * amount};
    sample.shoulder_r_delta = {0.0F, -0.03F * amount, 0.08F * amount};
    sample.neck_delta = {0.0F, -0.02F * amount, 0.09F * amount};
    sample.head_delta = {0.0F, -0.02F * amount, 0.06F * amount};
    break;
  case ShieldFormationPose::GuardDefault:
  case ShieldFormationPose::None:
    break;
  }

  return sample;
}

} // namespace Animation
