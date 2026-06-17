#pragma once

#include "combat_manifest.h"

namespace Animation {

struct HumanoidIntentInputs {
  MovementState locomotion{MovementState::Idle};
  bool is_mounted{false};
  bool is_in_hold_mode{false};
  bool is_exiting_hold{false};
  bool is_guarding{false};
  bool is_exiting_guard{false};
  bool has_chase_intent{false};

  bool is_dying{false};
  bool is_dead{false};
  bool is_hit_reacting{false};
  bool is_healing{false};
  bool is_constructing{false};

  bool is_attacking{false};
  bool is_melee{false};
  bool is_in_melee_lock{false};
  bool is_casting{false};
  CombatPhase combat_phase{CombatPhase::Idle};
  CombatAttackFamily attack_family{CombatAttackFamily::None};

  bool has_authoritative_combat{false};
  bool combat_active{false};
  bool combat_is_melee{false};
  bool combat_is_mounted{false};
  bool combat_is_casting{false};
  bool combat_prioritize_action_over_locomotion{false};
  CombatAttackFamily combat_attack_family{CombatAttackFamily::None};
};

struct HumanoidIntentResolution {
  SemanticIntent semantic{};
  CombatAttackFamily attack_family{CombatAttackFamily::None};
};

[[nodiscard]] auto is_stationary_melee_combat_phase(CombatPhase phase) noexcept -> bool;

[[nodiscard]] auto should_prioritize_humanoid_attack_pose(
    const HumanoidIntentInputs& inputs) noexcept -> bool;

[[nodiscard]] auto resolve_humanoid_intent(const HumanoidIntentInputs& inputs) noexcept
    -> HumanoidIntentResolution;

} // namespace Animation
