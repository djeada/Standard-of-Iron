#pragma once

#include <cstdint>

#include "pose_manifest.h"

namespace Animation {

struct HumanoidMicroIdlePoseInputs {
  float sample_time{0.0F};
  std::uint32_t seed{0U};
};

struct HumanoidPostureDeltaSample {
  float pelvis_x_delta{0.0F};
  float pelvis_y_delta{0.0F};
  float pelvis_z_delta{0.0F};
  float shoulder_l_x_delta{0.0F};
  float shoulder_l_y_delta{0.0F};
  float shoulder_l_z_delta{0.0F};
  float shoulder_r_x_delta{0.0F};
  float shoulder_r_y_delta{0.0F};
  float shoulder_r_z_delta{0.0F};
  float neck_x_delta{0.0F};
  float neck_y_delta{0.0F};
  float neck_z_delta{0.0F};
  float head_x_delta{0.0F};
  float head_y_delta{0.0F};
  float head_z_delta{0.0F};
  float hand_l_y_delta{0.0F};
  float hand_l_z_delta{0.0F};
  float hand_r_y_delta{0.0F};
  float hand_r_z_delta{0.0F};
  float knee_l_y_delta{0.0F};
  float knee_l_z_delta{0.0F};
  float knee_r_z_delta{0.0F};
  float foot_l_z_delta{0.0F};
  float foot_r_z_delta{0.0F};
  PoseVec3 torso_frame_origin_delta{};
  PoseVec3 head_frame_origin_delta{};
};

struct HumanoidKneelPoseInputs {
  float depth{0.0F};
  float waist_y{0.0F};
  float ground_y{0.0F};
  float lower_leg_len{0.0F};
  float foot_y_offset{0.0F};
};

struct HumanoidKneelPoseSample {
  bool active{false};
  PoseVec3 pelvis{};
  PoseVec3 knee_l{};
  PoseVec3 foot_l{};
  PoseVec3 knee_r{};
  PoseVec3 foot_r{};
  HumanoidPostureDeltaSample upper_body{};
};

struct HumanoidKneelTransitionPoseInputs {
  float progress{0.0F};
  bool standing_up{false};
};

struct HumanoidHitFlinchPoseInputs {
  float intensity{0.0F};
};

struct HumanoidLeanPoseInputs {
  PoseVec3 direction{};
  float amount{0.0F};
};

struct HumanoidLookAtPoseInputs {
  PoseVec3 head_position{};
  PoseVec3 target{};
};

struct HumanoidTorsoTiltPoseInputs {
  PoseVec3 heading_right{};
  PoseVec3 heading_forward{};
  float side_tilt{0.0F};
  float forward_tilt{0.0F};
};

struct MountedAxisDelta {
  float forward{0.0F};
  float right{0.0F};
  float up{0.0F};
  float world_y{0.0F};
};

struct MountedRiderPostureSample {
  bool active{true};
  MountedAxisDelta shoulder_l_delta{};
  MountedAxisDelta shoulder_r_delta{};
  MountedAxisDelta neck_delta{};
  MountedAxisDelta head_delta{};
  float head_forward_tilt{0.0F};
  float head_side_tilt{0.0F};
};

struct MountedRiderLeanPoseInputs {
  float forward_lean{0.0F};
  float side_lean{0.0F};
};

struct MountedRiderChargePoseInputs {
  float intensity{0.0F};
};

struct MountedRiderReiningPoseInputs {
  float left_tension{0.0F};
  float right_tension{0.0F};
};

struct MountedRiderTorsoSculptPoseInputs {
  float compression{0.0F};
  float twist{0.0F};
  float shoulder_dip{0.0F};
};

[[nodiscard]] auto resolve_humanoid_micro_idle_pose(
    const HumanoidMicroIdlePoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample;

[[nodiscard]] auto resolve_humanoid_kneel_pose(
    const HumanoidKneelPoseInputs& inputs) noexcept -> HumanoidKneelPoseSample;

[[nodiscard]] auto resolve_humanoid_kneel_transition_pose(
    const HumanoidKneelTransitionPoseInputs& inputs) noexcept
    -> HumanoidPostureDeltaSample;

[[nodiscard]] auto resolve_humanoid_hit_flinch_pose(
    const HumanoidHitFlinchPoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample;

[[nodiscard]] auto resolve_humanoid_lean_pose(
    const HumanoidLeanPoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample;

[[nodiscard]] auto resolve_humanoid_look_at_pose(
    const HumanoidLookAtPoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample;

[[nodiscard]] auto resolve_humanoid_torso_tilt_pose(
    const HumanoidTorsoTiltPoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample;

[[nodiscard]] auto resolve_mounted_rider_lean_pose(
    const MountedRiderLeanPoseInputs& inputs) noexcept -> MountedRiderPostureSample;

[[nodiscard]] auto resolve_mounted_rider_charge_pose(
    const MountedRiderChargePoseInputs& inputs) noexcept -> MountedRiderPostureSample;

[[nodiscard]] auto resolve_mounted_rider_reining_pose(
    const MountedRiderReiningPoseInputs& inputs) noexcept -> MountedRiderPostureSample;

[[nodiscard]] auto resolve_mounted_rider_torso_sculpt_pose(
    const MountedRiderTorsoSculptPoseInputs& inputs) noexcept
    -> MountedRiderPostureSample;

} // namespace Animation
