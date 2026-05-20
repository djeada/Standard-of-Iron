#pragma once

#include "../gl/humanoid/humanoid_types.h"
#include "../humanoid/humanoid_state_machine.h"
#include "pose_intent_enum.h"
#include "render_request.h"

namespace Render::Creature {

[[nodiscard]] inline constexpr auto
is_stationary_melee_combat_phase(Render::GL::CombatAnimPhase phase) noexcept -> bool {
  switch (phase) {
  case Render::GL::CombatAnimPhase::WindUp:
  case Render::GL::CombatAnimPhase::Strike:
  case Render::GL::CombatAnimPhase::Impact:
  case Render::GL::CombatAnimPhase::Recover:
    return true;
  case Render::GL::CombatAnimPhase::Idle:
  case Render::GL::CombatAnimPhase::Advance:
  case Render::GL::CombatAnimPhase::Reposition:
    return false;
  }
  return false;
}

[[nodiscard]] inline auto should_prioritize_attack_pose_over_locomotion(
    const Render::GL::AnimationInputs& inputs) noexcept -> bool {
  if (!inputs.is_attacking || !inputs.is_melee ||
      !Render::Creature::is_moving_animation(inputs.movement_state)) {
    return false;
  }

  if (inputs.is_in_melee_lock) {
    return true;
  }

  return is_stationary_melee_combat_phase(inputs.combat_phase) &&
         !inputs.visual_movement.has_chase_intent;
}

[[nodiscard]] inline constexpr auto
is_attack_pose_intent(PoseIntent intent) noexcept -> bool {
  switch (intent) {
  case PoseIntent::AttackMelee:
  case PoseIntent::AttackSpear:
  case PoseIntent::AttackRanged:
    return true;
  case PoseIntent::Idle:
  case PoseIntent::Walk:
  case PoseIntent::Run:
  case PoseIntent::Hold:
  case PoseIntent::HitReaction:
  case PoseIntent::Healing:
  case PoseIntent::Construct:
  case PoseIntent::RidingIdle:
  case PoseIntent::RidingCharge:
  case PoseIntent::RidingReining:
  case PoseIntent::RidingBowShot:
  case PoseIntent::Dying:
  case PoseIntent::Dead:
  case PoseIntent::Count:
    return false;
  }
  return false;
}

[[nodiscard]] inline constexpr auto
is_locomotion_pose_intent(PoseIntent intent) noexcept -> bool {
  switch (intent) {
  case PoseIntent::Walk:
  case PoseIntent::Run:
    return true;
  case PoseIntent::Idle:
  case PoseIntent::Hold:
  case PoseIntent::AttackMelee:
  case PoseIntent::AttackSpear:
  case PoseIntent::AttackRanged:
  case PoseIntent::HitReaction:
  case PoseIntent::Healing:
  case PoseIntent::Construct:
  case PoseIntent::RidingIdle:
  case PoseIntent::RidingCharge:
  case PoseIntent::RidingReining:
  case PoseIntent::RidingBowShot:
  case PoseIntent::Dying:
  case PoseIntent::Dead:
  case PoseIntent::Count:
    return false;
  }
  return false;
}

[[nodiscard]] inline constexpr auto
is_attack_animation_state(AnimationStateId state) noexcept -> bool {
  switch (state) {
  case AnimationStateId::AttackSword:
  case AnimationStateId::AttackSpear:
  case AnimationStateId::AttackBow:
    return true;
  case AnimationStateId::Idle:
  case AnimationStateId::Walk:
  case AnimationStateId::Run:
  case AnimationStateId::Hold:
  case AnimationStateId::Die:
  case AnimationStateId::Dead:
  case AnimationStateId::RidingIdle:
  case AnimationStateId::RidingCharge:
  case AnimationStateId::RidingReining:
  case AnimationStateId::RidingBowShot:
  case AnimationStateId::Count:
    return false;
  }
  return false;
}

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
  if (inputs.is_mounted && inputs.is_attacking && inputs.is_melee) {
    switch (inputs.attack_family) {
    case Engine::Core::CombatAttackFamily::Sword:
      return PoseIntent::AttackMelee;
    case Engine::Core::CombatAttackFamily::Spear:
    case Engine::Core::CombatAttackFamily::None:
    default:
      return PoseIntent::RidingCharge;
    }
  }
  if (!should_prioritize_attack_pose_over_locomotion(inputs)) {
    switch (inputs.movement_state) {
    case Render::Creature::MovementAnimationState::Run:
      return PoseIntent::Run;
    case Render::Creature::MovementAnimationState::Walk:
      return PoseIntent::Walk;
    case Render::Creature::MovementAnimationState::Idle:
      break;
    }
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

struct ResolvedPose {
  PoseIntent intent{PoseIntent::Idle};
  Render::GL::HumanoidMotionState motion_state{Render::GL::HumanoidMotionState::Idle};
  Render::Humanoid::HumanoidState humanoid_state{Render::Humanoid::HumanoidState::Idle};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
};

[[nodiscard]] inline auto
resolve_pose_for_intent(PoseIntent intent) noexcept -> ResolvedPose {
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

[[nodiscard]] inline auto
resolve_pose(const Render::GL::AnimationInputs& inputs) noexcept -> ResolvedPose {
  auto resolved_pose = resolve_pose_for_intent(resolve_pose_intent(inputs));
  if (inputs.is_attacking && inputs.is_in_hold_mode) {
    resolved_pose.animation_state = inputs.is_melee ? AnimationStateId::AttackMelee
                                                    : AnimationStateId::AttackRanged;
  }
  return resolved_pose;
}

[[nodiscard]] inline auto humanoid_motion_state_for_intent(PoseIntent intent) noexcept
    -> Render::GL::HumanoidMotionState {
  return resolve_pose_for_intent(intent).motion_state;
}

[[nodiscard]] inline auto humanoid_state_for_intent(PoseIntent intent) noexcept
    -> Render::Humanoid::HumanoidState {
  return resolve_pose_for_intent(intent).humanoid_state;
}

[[nodiscard]] inline auto animation_state_for_intent(PoseIntent intent) noexcept
    -> Render::Creature::AnimationStateId {
  return resolve_pose_for_intent(intent).animation_state;
}

} // namespace Render::Creature
