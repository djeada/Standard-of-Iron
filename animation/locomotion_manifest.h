#pragma once

#include "pose_manifest.h"

namespace Animation {

enum class HumanoidMotionState {
  Idle,
  Walk,
  Run,
  Hold,
  Attacking
};

inline constexpr float k_humanoid_walk_cycle_distance = 1.5833F;
inline constexpr float k_humanoid_run_cycle_distance = 2.8333F;

struct HumanoidLocomotionTuning {
  float reference_walk_speed{2.2F};
  float reference_run_speed{4.8F};
  float walk_speed_multiplier{1.0F};
  float idle_cycle_time{1.6F};
  float locomotion_blend_tau{0.12F};
  float cadence_blend_tau{0.14F};
  float run_blend_tau{0.16F};
  float turn_blend_tau{0.10F};
  float acceleration_blend_tau{0.14F};
};

struct HumanoidLocomotionPersistentState {
  bool initialized{false};
  float last_sample_time{0.0F};
  float phase_bias{0.0F};
  float cycle_time{0.0F};
  float phase{0.0F};
  float filtered_speed{0.0F};
  float filtered_acceleration{0.0F};
  float filtered_turn{0.0F};
  float filtered_travel_alignment{1.0F};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};

  float locomotion_presence{0.0F};
  float run_presence{0.0F};
  bool reverse_gait{false};
  HumanoidMotionState state{HumanoidMotionState::Idle};
};

struct HumanoidLocomotionInputs {
  MovementState movement_state{MovementState::Idle};
  HumanoidMotionState motion_state{HumanoidMotionState::Idle};
  float speed{0.0F};
  float entity_forward_x{0.0F};
  float entity_forward_z{1.0F};
  float locomotion_direction_x{0.0F};
  float locomotion_direction_z{1.0F};
  float sample_time{0.0F};
  float phase_offset{0.0F};
  HumanoidLocomotionTuning tuning{};
  bool has_persistent_state{false};
  HumanoidLocomotionPersistentState previous{};
  bool allow_persistent_update{false};
};

struct HumanoidLocomotionSample {
  HumanoidMotionState state{HumanoidMotionState::Idle};
  float speed{0.0F};
  float normalized_speed{0.0F};
  float cycle_time{0.0F};
  float cycle_phase{0.0F};
  float stride_distance{0.0F};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  float locomotion_presence{0.0F};
  float run_presence{0.0F};
  float turn_amount{0.0F};

  float travel_alignment{1.0F};
  bool reverse_gait{false};
  float acceleration{0.0F};
  bool write_persistent_state{false};
  HumanoidLocomotionPersistentState persistent{};
};

struct HumanoidLocomotionActionOverrideInputs {
  bool commander_jump_active{false};
};

struct HumanoidLocomotionActionOverrideSample {
  bool active{false};
  HumanoidMotionState state{HumanoidMotionState::Idle};
  float move_speed{0.0F};
  float normalized_speed{0.0F};
  bool has_target{false};
  bool airborne{false};
};

struct HumanoidLocomotionPhaseOverrideInputs {
  bool bow_ready_idle{false};
  bool has_locomotion{false};
  bool attacking{false};
};

struct HumanoidLocomotionPhaseOverrideSample {
  bool active{false};
  float cycle_phase{0.0F};
};

struct HumanoidLocomotionVariationInputs {
  bool has_locomotion{false};
  bool running{false};
  float walk_speed_multiplier{1.0F};
  float arm_swing_amplitude{1.0F};
  float stance_width{1.0F};
  float posture_slump{0.0F};
};

struct HumanoidLocomotionVariationSample {
  float walk_speed_multiplier{1.0F};
  float arm_swing_amplitude{1.0F};
  float stance_width{1.0F};
  float posture_slump{0.0F};
};

struct HumanoidBasePoseProportions {
  float chest_y{0.0F};
  float ground_y{0.0F};
  float head_center_y{0.0F};
  float head_radius{0.0F};
  float neck_base_y{0.0F};
  float shoulder_y{0.0F};
  float shoulder_width{0.0F};
  float waist_y{0.0F};
  float foot_y_offset_default{0.0F};
};

struct HumanoidBasePoseInputs {
  std::uint32_t seed{0U};
  float height_scale{1.0F};
  float bulk_scale{1.0F};
  float stance_width{1.0F};
  float posture_slump{0.0F};
  float shoulder_tilt{0.0F};
  HumanoidBasePoseProportions proportions{};
};

struct HumanoidBasePoseSample {
  PoseVec3 head_pos{};
  float head_radius{0.0F};
  PoseVec3 neck_base{};
  PoseVec3 shoulder_l{};
  PoseVec3 shoulder_r{};
  PoseVec3 pelvis{};
  float foot_y_offset{0.0F};
  PoseVec3 foot_l{};
  PoseVec3 foot_r{};
  PoseVec3 hand_l{};
  PoseVec3 hand_r{};
};

struct HumanoidLocomotionPoseInputs {
  HumanoidMotionState state{HumanoidMotionState::Idle};
  float normalized_speed{0.0F};
  float cycle_phase{0.0F};
  float stride_distance{0.0F};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  float turn_amount{0.0F};

  float travel_alignment{1.0F};
  float acceleration{0.0F};
  float walk_speed_multiplier{1.0F};
  float stance_width{1.0F};
  float arm_swing_amplitude{1.0F};
  float reference_walk_speed{2.35F};
  float reference_run_speed{5.10F};
  float ground_y{0.0F};
  float foot_y_offset{0.0F};
  PoseVec3 base_foot_l{};
  PoseVec3 base_foot_r{};

  float arm_pendulum_length{0.55F};
  float pelvis_y{0.0F};
  float hip_lateral_offset{0.0F};
  float hip_vertical_offset{0.0F};
  float leg_length{0.0F};
};

struct HumanoidLocomotionPoseSample {
  bool active{false};
  PoseVec3 foot_l{};
  PoseVec3 foot_r{};
  float foot_pitch_l{0.0F};
  float foot_pitch_r{0.0F};
  PoseVec3 pelvis_delta{};
  PoseVec3 hip_l_delta{};
  PoseVec3 hip_r_delta{};
  PoseVec3 shoulder_l_delta{};
  PoseVec3 shoulder_r_delta{};
  PoseVec3 neck_delta{};
  PoseVec3 head_delta{};
  PoseVec3 hand_l_delta{};
  PoseVec3 hand_r_delta{};
};

[[nodiscard]] auto wrap_locomotion_phase(float phase) noexcept -> float;

[[nodiscard]] auto humanoid_foot_contact_lift(float foot_pitch) noexcept -> float;

[[nodiscard]] auto humanoid_reference_cycle_time(
    HumanoidMotionState state,
    const HumanoidLocomotionTuning& tuning = {}) noexcept -> float;

[[nodiscard]] auto humanoid_walk_cycle_time_for_speed(float speed) noexcept -> float;

[[nodiscard]] auto humanoid_run_cycle_time_for_speed(float speed) noexcept -> float;

[[nodiscard]] auto resolve_humanoid_locomotion_sample(
    const HumanoidLocomotionInputs& inputs) noexcept -> HumanoidLocomotionSample;

[[nodiscard]] auto resolve_humanoid_locomotion_action_override(
    const HumanoidLocomotionActionOverrideInputs& inputs) noexcept
    -> HumanoidLocomotionActionOverrideSample;

[[nodiscard]] auto resolve_humanoid_locomotion_phase_override(
    const HumanoidLocomotionPhaseOverrideInputs& inputs) noexcept
    -> HumanoidLocomotionPhaseOverrideSample;

[[nodiscard]] auto resolve_humanoid_locomotion_variation(
    const HumanoidLocomotionVariationInputs& inputs) noexcept
    -> HumanoidLocomotionVariationSample;

[[nodiscard]] auto resolve_humanoid_base_pose(
    const HumanoidBasePoseInputs& inputs) noexcept -> HumanoidBasePoseSample;

[[nodiscard]] auto
resolve_humanoid_locomotion_pose(const HumanoidLocomotionPoseInputs& inputs) noexcept
    -> HumanoidLocomotionPoseSample;

} // namespace Animation
