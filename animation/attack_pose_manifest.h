#pragma once

#include <cstdint>

#include "mounted_pose_manifest.h"

namespace Animation {

enum class HumanoidWeaponAttackKind : std::uint8_t {
  BasicMeleeStrike,
  SwordSlash,
  CombatSwordSlash,
  SpearThrustClassic,
  SpearThrustFromHold,
  SpearThrust,
};

enum class MountedSpearGuardGrip : std::uint8_t {
  Overhand,
  Couched,
  TwoHanded,
};

enum class HumanoidConstructionPoseKind : std::uint8_t {
  Saw,
  Chisel,
  KneelingChisel,
};

struct HumanoidWeaponAttackPoseInputs {
  HumanoidWeaponAttackKind kind{HumanoidWeaponAttackKind::SwordSlash};
  float attack_phase{0.0F};
  std::uint8_t variant{0U};
  float reach_scale{1.0F};
  float hold_depth{0.0F};
  float attack_emphasis{1.0F};
  bool finisher_attack{false};
  float shoulder_y{0.0F};
  float waist_y{0.0F};
};

struct HumanoidSpearDirectionInputs {
  float hold_blend{0.0F};
  bool is_attacking{false};
  bool is_melee{false};
  float attack_phase{0.0F};
};

struct HumanoidWeaponAttackPoseSample {
  PoseVec3 right_hand{};
  PoseVec3 left_hand{};
  bool use_offhand_spear_grip{false};
  PoseVec3 offhand_spear_direction{0.05F, 0.55F, 0.85F};
  float offhand_along_offset{0.0F};
  float offhand_y_drop{0.0F};
  float offhand_lateral_offset{0.0F};

  float shoulder_l_x_delta{0.0F};
  float shoulder_r_x_delta{0.0F};
  float shoulder_l_y_delta{0.0F};
  float shoulder_r_y_delta{0.0F};
  float shoulder_l_z_delta{0.0F};
  float shoulder_r_z_delta{0.0F};
  float neck_y_delta{0.0F};
  float neck_z_delta{0.0F};
  float head_y_delta{0.0F};
  float head_z_delta{0.0F};
  float pelvis_y_delta{0.0F};
  float pelvis_z_delta{0.0F};
  float foot_l_z_delta{0.0F};
  float knee_l_z_delta{0.0F};
  float foot_r_z_delta{0.0F};
  float knee_r_z_delta{0.0F};
};

struct HumanoidBowDrawPoseInputs {
  float draw_phase{0.0F};
  float jitter_seed{0.0F};
  float shoulder_y{0.0F};
};

struct HumanoidBowDrawPoseSample {
  PoseVec3 right_hand{};
  PoseVec3 left_hand{};
  float shoulder_l_y_delta{0.0F};
  float shoulder_r_y_delta{0.0F};
  float shoulder_l_z_delta{0.0F};
  float shoulder_r_z_delta{0.0F};
  float neck_y_delta{0.0F};
  float neck_z_delta{0.0F};
  float head_z_delta{0.0F};
  float pelvis_z_delta{0.0F};
};

struct HumanoidConstructionPoseInputs {
  HumanoidConstructionPoseKind kind{HumanoidConstructionPoseKind::Saw};
  float work_phase{0.0F};
  float jitter_seed{0.0F};
  float shoulder_y{0.0F};
};

struct HumanoidConstructionPoseSample {
  bool use_two_handed_grip{false};
  PoseVec3 grip_center{};
  float hand_separation{0.0F};
  PoseVec3 right_hand{};
  PoseVec3 left_hand{};
  float shoulder_l_x_delta{0.0F};
  float shoulder_r_x_delta{0.0F};
  float shoulder_l_y_delta{0.0F};
  float shoulder_r_y_delta{0.0F};
  float shoulder_l_z_delta{0.0F};
  float shoulder_r_z_delta{0.0F};
  float neck_z_delta{0.0F};
  float head_y_delta{0.0F};
  float head_z_delta{0.0F};
  float pelvis_x_delta{0.0F};
  float foot_l_z_delta{0.0F};
  float foot_r_z_delta{0.0F};
  float knee_l_z_delta{0.0F};
  float knee_r_z_delta{0.0F};
};

struct MountedSpearThrustPoseInputs {
  float attack_phase{0.0F};
};

struct MountedSpearThrustPoseSample {
  MountedSeatOffset right_hand{};
  MountedSeatOffset left_hand{};
  float forward_lean{0.0F};
  float torso_twist{0.0F};
  float shoulder_drop{0.0F};
  float torso_compression{0.0F};
  float head_forward_tilt{0.0F};
  const char* debug_label{"mounted_spear"};
};

struct MountedSwordStrikePoseInputs {
  float attack_phase{0.0F};
};

struct MountedSwordStrikePoseSample {
  MountedSeatOffset right_hand{};
  float left_rein_slack{0.20F};
  float left_rein_tension{0.25F};
  float left_hand_up_offset{-0.02F};
  float torso_twist{0.0F};
  float side_lean{0.0F};
  float forward_lean{0.0F};
  float torso_commit{0.0F};
  float counter_lift{0.0F};
  float shoulder_dip{0.0F};
  float head_forward_tilt{0.0F};
  float head_side_tilt{0.0F};
  const char* debug_label{"sword_recover"};
};

struct MountedSpearGuardPoseInputs {
  MountedSpearGuardGrip grip{MountedSpearGuardGrip::Overhand};
};

struct MountedSpearGuardPoseSample {
  MountedSeatOffset right_hand{};
  MountedSeatOffset left_hand{};
  bool left_hand_uses_rein{false};
  float left_rein_slack{0.0F};
  float left_rein_tension{0.0F};
  const char* debug_label{"spear_guard_pose"};
};

struct MountedBowDrawPoseInputs {
  float draw_phase{0.0F};
};

struct MountedBowDrawPoseSample {
  MountedSeatOffset left_hand{};
  MountedSeatOffset right_hand{};
  float right_hand_world_y_offset{0.0F};
  const char* debug_label{"bow_draw"};
};

struct MountedShieldDefensePoseInputs {
  bool raised{false};
};

struct MountedShieldDefensePoseSample {
  MountedSeatOffset left_hand{};
  float right_rein_slack{0.30F};
  float right_rein_tension{0.25F};
  const char* debug_label{"shield_defense"};
};

[[nodiscard]] auto resolve_humanoid_weapon_attack_pose(
    const HumanoidWeaponAttackPoseInputs& inputs) noexcept
    -> HumanoidWeaponAttackPoseSample;

[[nodiscard]] auto resolve_humanoid_spear_direction(
    const HumanoidSpearDirectionInputs& inputs) noexcept -> PoseVec3;

[[nodiscard]] auto resolve_humanoid_bow_draw_pose(
    const HumanoidBowDrawPoseInputs& inputs) noexcept -> HumanoidBowDrawPoseSample;

[[nodiscard]] auto resolve_humanoid_construction_pose(
    const HumanoidConstructionPoseInputs& inputs) noexcept
    -> HumanoidConstructionPoseSample;

[[nodiscard]] auto
resolve_mounted_spear_thrust_pose(const MountedSpearThrustPoseInputs& inputs) noexcept
    -> MountedSpearThrustPoseSample;

[[nodiscard]] auto
resolve_mounted_sword_strike_pose(const MountedSwordStrikePoseInputs& inputs) noexcept
    -> MountedSwordStrikePoseSample;

[[nodiscard]] auto resolve_mounted_spear_guard_pose(
    const MountedSpearGuardPoseInputs& inputs) noexcept -> MountedSpearGuardPoseSample;

[[nodiscard]] auto resolve_mounted_bow_draw_pose(
    const MountedBowDrawPoseInputs& inputs) noexcept -> MountedBowDrawPoseSample;

[[nodiscard]] auto resolve_mounted_shield_defense_pose(
    const MountedShieldDefensePoseInputs& inputs) noexcept
    -> MountedShieldDefensePoseSample;

} // namespace Animation
