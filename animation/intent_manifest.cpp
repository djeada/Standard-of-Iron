#include "intent_manifest.h"

namespace Animation {

auto is_stationary_melee_combat_phase(CombatPhase phase) noexcept -> bool {
  switch (phase) {
  case CombatPhase::WindUp:
  case CombatPhase::Strike:
  case CombatPhase::Impact:
  case CombatPhase::Recover:
    return true;
  case CombatPhase::Idle:
  case CombatPhase::Advance:
  case CombatPhase::Reposition:
    return false;
  }
  return false;
}

auto should_prioritize_humanoid_attack_pose(const HumanoidIntentInputs& inputs) noexcept
    -> bool {
  if (!inputs.is_attacking || !inputs.is_melee || !is_moving(inputs.locomotion)) {
    return false;
  }

  if (inputs.is_in_melee_lock) {
    return true;
  }

  return is_stationary_melee_combat_phase(inputs.combat_phase) &&
         !inputs.has_chase_intent;
}

auto resolve_humanoid_intent(const HumanoidIntentInputs& inputs) noexcept
    -> HumanoidIntentResolution {
  HumanoidIntentResolution resolution{};
  auto& intent = resolution.semantic;
  intent.locomotion = inputs.locomotion;
  resolution.attack_family = inputs.has_authoritative_combat
                                 ? inputs.combat_attack_family
                                 : inputs.attack_family;

  if (inputs.is_mounted) {
    intent.stance = StanceIntent::Mounted;
  } else if (inputs.is_in_hold_mode || inputs.is_exiting_hold) {
    intent.stance = StanceIntent::Hold;
  } else if ((inputs.is_guarding || inputs.is_exiting_guard) &&
             !is_moving(inputs.locomotion)) {
    intent.stance = StanceIntent::Guard;
  }

  intent.prioritize_action_over_locomotion =
      inputs.has_authoritative_combat ? inputs.combat_prioritize_action_over_locomotion
                                      : should_prioritize_humanoid_attack_pose(inputs);

  if (inputs.is_dying) {
    intent.action = ActionIntent::Dying;
    return resolution;
  }
  if (inputs.is_dead) {
    intent.action = ActionIntent::Dead;
    return resolution;
  }
  if (inputs.is_hit_reacting) {
    intent.action = ActionIntent::HitReaction;
    return resolution;
  }

  bool const combat_attacking =
      inputs.has_authoritative_combat ? inputs.combat_active : inputs.is_attacking;
  bool const combat_melee =
      inputs.has_authoritative_combat ? inputs.combat_is_melee : inputs.is_melee;
  bool const combat_mounted =
      inputs.has_authoritative_combat ? inputs.combat_is_mounted : inputs.is_mounted;
  bool const combat_casting =
      inputs.has_authoritative_combat ? inputs.combat_is_casting : inputs.is_casting;
  CombatAttackFamily const attack_family = resolution.attack_family;

  if (combat_mounted && combat_attacking && combat_melee) {
    intent.prioritize_action_over_locomotion = true;
    switch (attack_family) {
    case CombatAttackFamily::Sword:
      intent.action = ActionIntent::AttackMelee;
      break;
    case CombatAttackFamily::Spear:
    case CombatAttackFamily::Bow:
    case CombatAttackFamily::None:
    default:
      intent.action = ActionIntent::MountedCharge;
      break;
    }
    return resolution;
  }

  if (combat_attacking) {
    if (combat_casting) {
      intent.action = ActionIntent::Cast;
      return resolution;
    }
    if (inputs.is_in_hold_mode) {
      intent.action =
          inputs.is_melee ? ActionIntent::AttackMelee : ActionIntent::AttackRanged;
      return resolution;
    }
    switch (attack_family) {
    case CombatAttackFamily::Spear:
      intent.action = ActionIntent::AttackSpear;
      return resolution;
    case CombatAttackFamily::Bow:
      intent.action = ActionIntent::AttackRanged;
      return resolution;
    case CombatAttackFamily::Sword:
      intent.action = ActionIntent::AttackMelee;
      return resolution;
    case CombatAttackFamily::None:
    default:
      intent.action =
          inputs.is_melee ? ActionIntent::AttackMelee : ActionIntent::AttackRanged;
      return resolution;
    }
  }

  if (inputs.is_healing) {
    intent.action = ActionIntent::Healing;
    return resolution;
  }
  if (inputs.is_constructing) {
    intent.action = ActionIntent::Construct;
    return resolution;
  }
  return resolution;
}

} // namespace Animation
