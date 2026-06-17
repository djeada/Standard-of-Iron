#pragma once

#include <cstdint>

#include "pose_manifest.h"

namespace Animation {

struct MountedSeatOffset {
  float forward{0.0F};
  float right{0.0F};
  float up{0.0F};
};

enum class MountedHandAnchor : std::uint8_t {
  Seat,
  LeftShoulder,
  RightShoulder,
};

enum class MountedRiderHandPoseKind : std::uint8_t {
  IdleRest,
  ReinHold,
  ShieldStowed,
  SwordIdle,
};

enum class MountedSeatPoseKind : std::uint8_t {
  Neutral,
  Forward,
  Defensive,
};

enum class MountedWeaponPoseKind : std::uint8_t {
  None,
  SwordIdle,
  SwordStrike,
  SpearGuard,
  SpearThrust,
  BowDraw,
};

enum class MountedShieldPoseKind : std::uint8_t {
  None,
  Stowed,
  Guard,
  Raised,
};

struct MountedElbowProfile {
  float along_frac{0.45F};
  float lateral_offset{0.12F};
  float y_bias{-0.08F};
  float outward_sign{1.0F};
};

struct MountedRiderHandPoseInputs {
  MountedRiderHandPoseKind kind{MountedRiderHandPoseKind::IdleRest};
  float body_length{0.0F};
  float body_width{0.0F};
  float body_height{0.0F};
  float saddle_thickness{0.0F};
  float left_rein_slack{0.20F};
  float right_rein_slack{0.20F};
  float left_rein_tension{0.25F};
  float right_rein_tension{0.25F};
};

struct MountedHandPoseTarget {
  bool active{false};
  bool uses_rein{false};
  MountedHandAnchor anchor{MountedHandAnchor::Seat};
  MountedSeatOffset offset{};
  float rein_slack{0.0F};
  float rein_tension{0.0F};
  MountedElbowProfile elbow{};
};

struct MountedRiderHandPoseSample {
  MountedHandPoseTarget left{};
  MountedHandPoseTarget right{};
  const char* debug_label{"mounted_hands"};
};

struct MountedRiderPosePlanInputs {
  MountedSeatPoseKind seat_pose{MountedSeatPoseKind::Neutral};
  MountedWeaponPoseKind weapon_pose{MountedWeaponPoseKind::None};
  MountedShieldPoseKind shield_pose{MountedShieldPoseKind::None};
  float forward_bias{0.0F};
  bool left_hand_on_reins{true};
  bool right_hand_on_reins{true};
};

struct MountedRiderPosePlan {
  float forward_bias{0.0F};
  bool apply_left_rein{false};
  bool apply_right_rein{false};
  bool weapon_claims_left_hand{false};
  bool weapon_claims_right_hand{false};
  bool shield_claims_left_hand{false};
  bool sword_strike_keeps_left_hand{false};
};

[[nodiscard]] auto resolve_mounted_rider_hand_pose(
    const MountedRiderHandPoseInputs& inputs) noexcept -> MountedRiderHandPoseSample;

[[nodiscard]] auto resolve_mounted_rider_pose_plan(
    const MountedRiderPosePlanInputs& inputs) noexcept -> MountedRiderPosePlan;

} // namespace Animation
