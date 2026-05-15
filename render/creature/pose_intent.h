

#pragma once

#include "../gl/humanoid/humanoid_types.h"
#include "../humanoid/humanoid_state_machine.h"
#include "pose_intent_enum.h"
#include "render_request.h"

namespace Render::Creature {

[[nodiscard]] inline auto
resolve_pose_intent(const Render::GL::AnimationInputs& inputs) noexcept -> PoseIntent {
  if (inputs.is_dying) {
    return PoseIntent::Dying;
  }
  if (inputs.is_dead) {
    return PoseIntent::Dead;
  }
  if (inputs.is_hit_reacting) {
    return PoseIntent::HitReaction;
  }
  if (inputs.is_attacking) {
    if (inputs.is_in_hold_mode) {
      return inputs.is_melee ? PoseIntent::AttackMelee : PoseIntent::AttackRanged;
    }
    switch (inputs.attack_family) {
    case Engine::Core::CombatAttackFamily::Spear:
      return PoseIntent::AttackSpear;
    case Engine::Core::CombatAttackFamily::Bow:
      return PoseIntent::AttackRanged;
    case Engine::Core::CombatAttackFamily::Sword:
      return PoseIntent::AttackMelee;
    case Engine::Core::CombatAttackFamily::None:
    default:
      return inputs.is_melee ? PoseIntent::AttackMelee : PoseIntent::AttackRanged;
    }
  }
  if (inputs.is_healing) {
    return PoseIntent::Healing;
  }
  if (inputs.is_constructing) {
    return PoseIntent::Construct;
  }
  if (inputs.is_in_hold_mode || inputs.is_exiting_hold ||
      (inputs.is_guarding && !inputs.is_moving && !inputs.is_running)) {
    return PoseIntent::Hold;
  }
  if (inputs.is_running) {
    return PoseIntent::Run;
  }
  if (inputs.is_moving) {
    return PoseIntent::Walk;
  }
  return PoseIntent::Idle;
}

[[nodiscard]] inline auto
to_humanoid_state(PoseIntent intent) noexcept -> Render::Humanoid::HumanoidState {
  using S = Render::Humanoid::HumanoidState;
  switch (intent) {
  case PoseIntent::Walk:
    return S::Walk;
  case PoseIntent::Run:
    return S::Run;
  case PoseIntent::Hold:
    return S::Hold;
  case PoseIntent::AttackMelee:
    return S::AttackMelee;
  case PoseIntent::AttackSpear:
    return S::AttackMelee;
  case PoseIntent::AttackRanged:
    return S::AttackRanged;
  case PoseIntent::HitReaction:
    return S::HitReaction;
  case PoseIntent::Healing:
    return S::Healing;
  case PoseIntent::Construct:
    return S::Construct;
  case PoseIntent::Dying:
    return S::Death;
  case PoseIntent::Dead:
    return S::Death;
  default:
    return S::Idle;
  }
}

[[nodiscard]] inline auto to_animation_state_id(PoseIntent intent) noexcept
    -> Render::Creature::AnimationStateId {
  using S = Render::Creature::AnimationStateId;
  switch (intent) {
  case PoseIntent::Walk:
    return S::Walk;
  case PoseIntent::Run:
    return S::Run;
  case PoseIntent::Hold:
    return S::Hold;
  case PoseIntent::AttackMelee:
    return S::AttackSword;
  case PoseIntent::AttackSpear:
    return S::AttackSpear;
  case PoseIntent::AttackRanged:
    return S::AttackBow;
  case PoseIntent::Construct:
    return S::AttackSword;
  case PoseIntent::Dying:
    return S::Die;
  case PoseIntent::Dead:
    return S::Dead;
  case PoseIntent::RidingIdle:
    return S::RidingIdle;
  case PoseIntent::RidingCharge:
    return S::RidingCharge;
  case PoseIntent::RidingReining:
    return S::RidingReining;
  case PoseIntent::RidingBowShot:
    return S::RidingBowShot;
  default:
    return S::Idle;
  }
}

} // namespace Render::Creature
