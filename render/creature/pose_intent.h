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
  switch (inputs.movement_state) {
  case Render::Creature::MovementAnimationState::Run:
    return PoseIntent::Run;
  case Render::Creature::MovementAnimationState::Walk:
    return PoseIntent::Walk;
  case Render::Creature::MovementAnimationState::Idle:
    break;
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
      (inputs.is_guarding &&
       !Render::Creature::is_moving_animation(inputs.movement_state))) {
    return PoseIntent::Hold;
  }

  return PoseIntent::Idle;
}

struct PoseClassification {
  PoseIntent intent{PoseIntent::Idle};
  Render::GL::HumanoidMotionState motion_state{Render::GL::HumanoidMotionState::Idle};
  Render::Humanoid::HumanoidState humanoid_state{Render::Humanoid::HumanoidState::Idle};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
};

[[nodiscard]] inline auto
classify_pose(PoseIntent intent) noexcept -> PoseClassification {
  using A = Render::Creature::AnimationStateId;
  using H = Render::Humanoid::HumanoidState;
  using M = Render::GL::HumanoidMotionState;

  switch (intent) {
  case PoseIntent::Walk:
    return {intent, M::Walk, H::Walk, A::Walk};
  case PoseIntent::Run:
    return {intent, M::Run, H::Run, A::Run};
  case PoseIntent::Hold:
    return {intent, M::Hold, H::Hold, A::Hold};
  case PoseIntent::AttackMelee:
    return {intent, M::Attacking, H::AttackMelee, A::AttackSword};
  case PoseIntent::AttackSpear:
    return {intent, M::Attacking, H::AttackMelee, A::AttackSpear};
  case PoseIntent::AttackRanged:
    return {intent, M::Attacking, H::AttackRanged, A::AttackBow};
  case PoseIntent::HitReaction:
    return {intent, M::Idle, H::HitReaction, A::Idle};
  case PoseIntent::Healing:
    return {intent, M::Idle, H::Healing, A::Idle};
  case PoseIntent::Construct:
    return {intent, M::Idle, H::Construct, A::AttackSword};
  case PoseIntent::Dying:
    return {intent, M::Idle, H::Death, A::Die};
  case PoseIntent::Dead:
    return {intent, M::Idle, H::Death, A::Dead};
  case PoseIntent::RidingIdle:
    return {intent, M::Idle, H::Idle, A::RidingIdle};
  case PoseIntent::RidingCharge:
    return {intent, M::Idle, H::Idle, A::RidingCharge};
  case PoseIntent::RidingReining:
    return {intent, M::Idle, H::Idle, A::RidingReining};
  case PoseIntent::RidingBowShot:
    return {intent, M::Idle, H::Idle, A::RidingBowShot};
  default:
    return {PoseIntent::Idle, M::Idle, H::Idle, A::Idle};
  }
}

[[nodiscard]] inline auto classify_pose(
    const Render::GL::AnimationInputs& inputs) noexcept -> PoseClassification {
  return classify_pose(resolve_pose_intent(inputs));
}

[[nodiscard]] inline auto to_humanoid_motion_state(PoseIntent intent) noexcept
    -> Render::GL::HumanoidMotionState {
  return classify_pose(intent).motion_state;
}

[[nodiscard]] inline auto
to_humanoid_state(PoseIntent intent) noexcept -> Render::Humanoid::HumanoidState {
  return classify_pose(intent).humanoid_state;
}

[[nodiscard]] inline auto to_animation_state_id(PoseIntent intent) noexcept
    -> Render::Creature::AnimationStateId {
  return classify_pose(intent).animation_state;
}

} // namespace Render::Creature
