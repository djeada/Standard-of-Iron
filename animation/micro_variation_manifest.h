#pragma once

#include <cstdint>

#include "combat_manifest.h"

namespace Animation {

struct HumanoidCombatMicroVariationInputs {
  bool multi_soldier_unit{false};
  bool authoritative_combat{false};
  bool is_dying{false};
  bool is_dead{false};
  SoldierCombatLane lane{SoldierCombatLane::None};
  CombatPhase combat_phase{CombatPhase::Idle};
  bool is_attacking{false};
  float attack_phase{0.0F};
  float sample_time{0.0F};
  std::uint32_t inst_seed{0U};
};

struct HumanoidCombatMicroVariation {
  bool active{false};
  float breath{0.0F};
  float lane_sign{1.0F};
  float torso_lean{0.0F};
  float lateral_tilt{0.0F};
  float shoulder_delay{0.0F};
  float wrist_angle{0.0F};
  float foot_adjust{0.0F};
  float shield_reaction{0.0F};
  float head_tracking{0.0F};
  float impact_stagger{0.0F};
};

struct HumanoidHitReactionTransformInputs {
  bool active{false};
  float intensity{0.0F};
  float recoil_x{0.0F};
  float recoil_z{0.0F};
  std::uint32_t inst_seed{0U};
};

struct HumanoidHitReactionTransformSample {
  bool active{false};
  float recoil_x{0.0F};
  float recoil_z{0.0F};
  float squash{0.0F};
  float tilt_degrees{0.0F};
};

[[nodiscard]] auto resolve_humanoid_combat_micro_variation(
    const HumanoidCombatMicroVariationInputs& inputs) noexcept
    -> HumanoidCombatMicroVariation;

[[nodiscard]] auto resolve_humanoid_hit_reaction_transform(
    const HumanoidHitReactionTransformInputs& inputs) noexcept
    -> HumanoidHitReactionTransformSample;

} // namespace Animation
