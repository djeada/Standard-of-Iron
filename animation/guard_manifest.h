#pragma once

#include <cstdint>

namespace Animation {

enum class ShieldFormationPose : std::uint8_t {
  None = 0,
  GuardDefault,
  RomanFront,
  RomanTop,
  CarthageFront,
};

enum class GuardShieldFamily : std::uint8_t {
  None = 0,
  Roman,
  Carthage,
};

struct HumanoidGuardShieldPoseInputs {
  bool has_left_hand_shield{false};
  bool infantry_formation_unit{false};
  bool formation_active{false};
  bool guard_mode_active{false};
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

[[nodiscard]] constexpr auto
is_interior_formation_slot(int row, int col, int rows, int cols) noexcept -> bool {
  return rows > 2 && cols > 2 && row > 0 && row + 1 < rows && col > 0 && col + 1 < cols;
}

[[nodiscard]] constexpr auto resolve_humanoid_guard_shield_pose(
    const HumanoidGuardShieldPoseInputs& inputs) noexcept -> ShieldFormationPose {
  if (!inputs.has_left_hand_shield) {
    return ShieldFormationPose::None;
  }

  switch (inputs.shield_family) {
  case GuardShieldFamily::Roman:
    if (inputs.infantry_formation_unit &&
        (inputs.formation_active || inputs.guard_mode_active) &&
        is_interior_formation_slot(inputs.row, inputs.col, inputs.rows, inputs.cols)) {
      return ShieldFormationPose::RomanTop;
    }
    return ShieldFormationPose::RomanFront;
  case GuardShieldFamily::Carthage:
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
    profile.pitch_degrees = -8.0F;
    profile.translate_y = 0.06F;
    profile.translate_z = 0.06F;
    break;
  case ShieldFormationPose::RomanTop:
    profile.yaw_degrees = 180.0F;
    profile.pitch_degrees = -78.0F;
    profile.translate_y = 0.20F;
    profile.translate_z = -0.03F;
    break;
  case ShieldFormationPose::CarthageFront:
    profile.yaw_degrees = 180.0F;
    profile.pitch_degrees = -40.0F;
    profile.translate_y = 0.14F;
    profile.translate_z = 0.03F;
    break;
  case ShieldFormationPose::GuardDefault:
  case ShieldFormationPose::None:
    break;
  }
  return profile;
}

} // namespace Animation
