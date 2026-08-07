#pragma once

#include <cstdint>

namespace Animation {

enum class ShieldFormationPose : std::uint8_t {
  None = 0,
  GuardDefault,
  RomanFront,
  RomanTop,
  CarthageFront,
  RomanLeft,
  RomanRight,
  RomanRear,
  CarthageLeft,
  CarthageRight,
};

enum class GuardShieldFamily : std::uint8_t {
  None = 0,
  Roman,
  Carthage,
};

enum class FormationShieldFacing : std::uint8_t {
  Front = 0,
  Left,
  Right,
  Rear,
  Top,
};

struct HumanoidGuardShieldPoseInputs {
  bool has_left_hand_shield{false};
  bool infantry_formation_unit{false};
  bool formation_active{false};
  bool guard_mode_active{false};
  bool defensive_layout_locked{false};
  GuardShieldFamily shield_family{GuardShieldFamily::None};
  int row{0};
  int col{0};
  int rows{1};
  int cols{1};
};

struct GuardShieldAttachmentProfile {
  float base_yaw_degrees{-90.0F};
  float yaw_degrees{0.0F};
  float pitch_degrees{0.0F};
  float translate_y{0.0F};
  float translate_z{0.0F};
};

[[nodiscard]] constexpr auto resolve_formation_shield_facing(
    int row, int col, int rows, int cols) noexcept -> FormationShieldFacing {

  if (rows <= 1 || row + 1 >= rows) {
    return FormationShieldFacing::Front;
  }
  if (row <= 0) {
    return FormationShieldFacing::Rear;
  }
  if (col <= 0 && cols > 1) {
    return FormationShieldFacing::Left;
  }
  if (cols > 1 && col + 1 >= cols) {
    return FormationShieldFacing::Right;
  }
  return FormationShieldFacing::Top;
}

[[nodiscard]] constexpr auto resolve_humanoid_guard_shield_pose(
    const HumanoidGuardShieldPoseInputs& inputs) noexcept -> ShieldFormationPose {
  if (!inputs.has_left_hand_shield) {
    return ShieldFormationPose::None;
  }

  bool const defensive_layout = inputs.infantry_formation_unit &&
                                inputs.defensive_layout_locked &&
                                (inputs.formation_active || inputs.guard_mode_active);
  auto const facing =
      resolve_formation_shield_facing(inputs.row, inputs.col, inputs.rows, inputs.cols);

  switch (inputs.shield_family) {
  case GuardShieldFamily::Roman:
    if (!defensive_layout) {
      return ShieldFormationPose::RomanFront;
    }
    switch (facing) {
    case FormationShieldFacing::Top:
      return ShieldFormationPose::RomanTop;
    case FormationShieldFacing::Left:
      return ShieldFormationPose::RomanLeft;
    case FormationShieldFacing::Right:
      return ShieldFormationPose::RomanRight;
    case FormationShieldFacing::Rear:
      return ShieldFormationPose::RomanRear;
    case FormationShieldFacing::Front:
      break;
    }
    return ShieldFormationPose::RomanFront;
  case GuardShieldFamily::Carthage:
    if (!defensive_layout) {
      return ShieldFormationPose::CarthageFront;
    }
    switch (facing) {
    case FormationShieldFacing::Left:
      return ShieldFormationPose::CarthageLeft;
    case FormationShieldFacing::Right:
      return ShieldFormationPose::CarthageRight;
    case FormationShieldFacing::Front:
    case FormationShieldFacing::Rear:
    case FormationShieldFacing::Top:
      break;
    }
    return ShieldFormationPose::CarthageFront;
  case GuardShieldFamily::None:
    break;
  }
  return ShieldFormationPose::None;
}

[[nodiscard]] constexpr auto guard_shield_attachment_profile(
    ShieldFormationPose pose) noexcept -> GuardShieldAttachmentProfile {
  GuardShieldAttachmentProfile profile{};
  switch (pose) {
  case ShieldFormationPose::RomanFront:
    profile.yaw_degrees = 180.0F;
    profile.pitch_degrees = -4.0F;
    profile.translate_y = -0.04F;
    profile.translate_z = 0.08F;
    break;
  case ShieldFormationPose::RomanTop:
    profile.yaw_degrees = 180.0F;
    profile.pitch_degrees = -88.0F;
    profile.translate_y = 0.20F;
    profile.translate_z = 0.14F;
    break;
  case ShieldFormationPose::RomanLeft:
    profile.yaw_degrees = 90.0F;
    profile.pitch_degrees = -6.0F;
    profile.translate_y = 0.10F;
    profile.translate_z = 0.02F;
    break;
  case ShieldFormationPose::RomanRight:
    profile.yaw_degrees = 270.0F;
    profile.pitch_degrees = -6.0F;
    profile.translate_y = 0.10F;
    profile.translate_z = 0.02F;
    break;
  case ShieldFormationPose::RomanRear:
    profile.yaw_degrees = 0.0F;
    profile.pitch_degrees = -8.0F;
    profile.translate_y = 0.08F;
    profile.translate_z = -0.06F;
    break;
  case ShieldFormationPose::CarthageFront:
    profile.yaw_degrees = 180.0F;
    profile.pitch_degrees = -40.0F;
    profile.translate_y = 0.14F;
    profile.translate_z = 0.03F;
    break;
  case ShieldFormationPose::CarthageLeft:
    profile.yaw_degrees = 125.0F;
    profile.pitch_degrees = -30.0F;
    profile.translate_y = 0.12F;
    profile.translate_z = 0.02F;
    break;
  case ShieldFormationPose::CarthageRight:
    profile.yaw_degrees = 235.0F;
    profile.pitch_degrees = -30.0F;
    profile.translate_y = 0.12F;
    profile.translate_z = 0.02F;
    break;
  case ShieldFormationPose::GuardDefault:
  case ShieldFormationPose::None:
    break;
  }
  return profile;
}

} // namespace Animation
