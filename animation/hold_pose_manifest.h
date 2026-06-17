#pragma once

#include <cstdint>

#include "attack_pose_manifest.h"
#include "guard_manifest.h"

namespace Animation {

enum class HumanoidHeldPoseKind : std::uint8_t {
  SpearIdle,
  SpearBrace,
  BowReady,
  SwordShieldHold,
};

struct HumanoidHeldPoseInputs {
  HumanoidHeldPoseKind kind{HumanoidHeldPoseKind::BowReady};
  float shoulder_y{0.0F};
  bool moving{false};
};

struct HumanoidHeldPoseSample {
  PoseVec3 right_hand{};
  PoseVec3 left_hand{};
  bool use_offhand_spear_grip{false};
  PoseVec3 offhand_spear_direction{0.05F, 0.55F, 0.85F};
  float offhand_along_offset{0.0F};
  float offhand_y_drop{0.0F};
  float offhand_lateral_offset{0.0F};
  bool clamp_left_hand_x_min{false};
  float left_hand_x_min{0.0F};
  bool clamp_left_hand_y_max{false};
  float left_hand_y_max{0.0F};
  float left_hand_z_delta{0.0F};

  float shoulder_l_x_delta{0.0F};
  float shoulder_r_x_delta{0.0F};
  float shoulder_l_y_delta{0.0F};
  float shoulder_r_y_delta{0.0F};
  float shoulder_l_z_delta{0.0F};
  float shoulder_r_z_delta{0.0F};
  float neck_z_delta{0.0F};
  float head_y_delta{0.0F};
  float head_z_delta{0.0F};
};

struct HumanoidGuardStanceInputs {
  ShieldFormationPose pose{ShieldFormationPose::GuardDefault};
  float amount{1.0F};
  float shoulder_y{0.0F};
};

struct HumanoidGuardStanceSample {
  bool active{false};
  float blend_amount{0.0F};
  PoseVec3 right_hand{};
  PoseVec3 left_hand{};
  PoseVec3 shoulder_l_delta{};
  PoseVec3 shoulder_r_delta{};
  PoseVec3 neck_delta{};
  PoseVec3 head_delta{};
};

[[nodiscard]] auto resolve_humanoid_held_pose(
    const HumanoidHeldPoseInputs& inputs) noexcept -> HumanoidHeldPoseSample;

[[nodiscard]] auto resolve_humanoid_guard_stance_pose(
    const HumanoidGuardStanceInputs& inputs) noexcept -> HumanoidGuardStanceSample;

} // namespace Animation
