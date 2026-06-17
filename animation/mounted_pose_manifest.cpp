#include "mounted_pose_manifest.h"

#include <algorithm>

namespace Animation {

namespace {

[[nodiscard]] auto clamp01(float value) noexcept -> float {
  return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

auto resolve_mounted_rider_hand_pose(const MountedRiderHandPoseInputs& inputs) noexcept
    -> MountedRiderHandPoseSample {
  MountedRiderHandPoseSample sample{};

  switch (inputs.kind) {
  case MountedRiderHandPoseKind::IdleRest:
    sample.left = {
        .active = true,
        .anchor = MountedHandAnchor::Seat,
        .offset = {0.12F, -0.14F, -0.05F},
        .elbow = {.along_frac = 0.45F, .lateral_offset = 0.12F, .y_bias = -0.05F},
    };
    sample.right = {
        .active = true,
        .anchor = MountedHandAnchor::Seat,
        .offset = {0.12F, 0.14F, -0.05F},
        .elbow = {.along_frac = 0.45F, .lateral_offset = 0.12F, .y_bias = -0.05F},
    };
    sample.debug_label = "riding_idle";
    break;
  case MountedRiderHandPoseKind::ReinHold:
    sample.left = {
        .active = true,
        .uses_rein = true,
        .rein_slack = clamp01(inputs.left_rein_slack),
        .rein_tension = clamp01(inputs.left_rein_tension),
        .elbow = {.along_frac = 0.45F, .lateral_offset = 0.12F, .y_bias = -0.08F},
    };
    sample.right = {
        .active = true,
        .uses_rein = true,
        .rein_slack = clamp01(inputs.right_rein_slack),
        .rein_tension = clamp01(inputs.right_rein_tension),
        .elbow = {.along_frac = 0.45F, .lateral_offset = 0.12F, .y_bias = -0.08F},
    };
    sample.debug_label = "hold_reins";
    break;
  case MountedRiderHandPoseKind::ShieldStowed:
    sample.left = {
        .active = true,
        .anchor = MountedHandAnchor::Seat,
        .offset = {inputs.body_length * -0.05F,
                   inputs.body_width * -0.55F,
                   inputs.saddle_thickness * 0.50F},
        .elbow = {.along_frac = 0.42F, .lateral_offset = 0.12F, .y_bias = -0.05F},
    };
    sample.debug_label = "shield_stowed";
    break;
  case MountedRiderHandPoseKind::SwordIdle:
    sample.right = {
        .active = true,
        .anchor = MountedHandAnchor::RightShoulder,
        .offset = {inputs.body_length * 0.22F,
                   inputs.body_width * 0.90F,
                   inputs.body_height * 0.06F + inputs.saddle_thickness * 0.10F},
        .elbow = {.along_frac = 0.46F, .lateral_offset = 0.24F, .y_bias = -0.05F},
    };
    sample.debug_label = "sword_idle";
    break;
  }

  return sample;
}

auto resolve_mounted_rider_pose_plan(const MountedRiderPosePlanInputs& inputs) noexcept
    -> MountedRiderPosePlan {
  MountedRiderPosePlan plan{};
  plan.forward_bias = inputs.forward_bias;

  switch (inputs.seat_pose) {
  case MountedSeatPoseKind::Forward:
    plan.forward_bias += 0.35F;
    break;
  case MountedSeatPoseKind::Defensive:
    plan.forward_bias -= 0.20F;
    break;
  case MountedSeatPoseKind::Neutral:
    break;
  }

  switch (inputs.weapon_pose) {
  case MountedWeaponPoseKind::SpearGuard:
  case MountedWeaponPoseKind::SpearThrust:
  case MountedWeaponPoseKind::BowDraw:
    plan.weapon_claims_left_hand = true;
    [[fallthrough]];
  case MountedWeaponPoseKind::SwordIdle:
  case MountedWeaponPoseKind::SwordStrike:
    plan.weapon_claims_right_hand = true;
    break;
  case MountedWeaponPoseKind::None:
    break;
  }

  plan.shield_claims_left_hand = inputs.shield_pose != MountedShieldPoseKind::None;
  plan.apply_left_rein = inputs.left_hand_on_reins && !plan.shield_claims_left_hand &&
                         !plan.weapon_claims_left_hand;
  plan.apply_right_rein = inputs.right_hand_on_reins && !plan.weapon_claims_right_hand;
  plan.sword_strike_keeps_left_hand = plan.shield_claims_left_hand;
  return plan;
}

} // namespace Animation
