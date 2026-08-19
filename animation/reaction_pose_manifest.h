#pragma once

#include <cstdint>

#include "pose_manifest.h"

namespace Animation {

enum class HumanoidReactionKind : std::uint8_t {
  Flinch = 0,
  Block = 1,
  Evade = 2,
  Stagger = 3,
};

enum class HumanoidReadyWeapon : std::uint8_t {
  None = 0,
  SwordAndShield,
  Spear,
};

struct HumanoidReadyStanceInputs {
  float phase{0.0F};
  HumanoidReadyWeapon weapon{HumanoidReadyWeapon::None};
};

struct HumanoidReadyStanceSample {
  float crouch{0.0F};
  float torso_forward{0.0F};
  float torso_side{0.0F};
  float sway_x{0.0F};
  float weapon_phase{0.0F};
  float shield_raise{0.0F};
};

struct HumanoidReactionPoseInputs {
  HumanoidReactionKind kind{HumanoidReactionKind::Flinch};
  float phase{0.0F};
  HumanoidReadyWeapon weapon{HumanoidReadyWeapon::None};
};

struct HumanoidReactionPoseSample {
  float envelope{0.0F};
  float crouch{0.0F};
  float torso_forward{0.0F};
  float torso_side{0.0F};
  float flinch{0.0F};
  float shield_raise{0.0F};
  float head_back{0.0F};
  float weapon_phase{0.0F};
  PoseVec3 hand_l_delta{};
  PoseVec3 hand_r_delta{};
};

[[nodiscard]] auto reaction_out_and_back(float phase,
                                         float out_fraction) noexcept -> float;

[[nodiscard]] auto resolve_humanoid_ready_stance(
    const HumanoidReadyStanceInputs& inputs) noexcept -> HumanoidReadyStanceSample;

[[nodiscard]] auto resolve_humanoid_reaction_pose(
    const HumanoidReactionPoseInputs& inputs) noexcept -> HumanoidReactionPoseSample;

[[nodiscard]] constexpr auto
humanoid_reaction_duration(HumanoidReactionKind kind) noexcept -> float {
  switch (kind) {
  case HumanoidReactionKind::Flinch:
    return 0.30F;
  case HumanoidReactionKind::Block:
    return 0.34F;
  case HumanoidReactionKind::Evade:
    return 0.36F;
  case HumanoidReactionKind::Stagger:
    return 0.60F;
  }
  return 0.30F;
}

} // namespace Animation
