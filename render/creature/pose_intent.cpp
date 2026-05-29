#include "pose_intent.h"

namespace Render::Creature {

namespace {

auto resolved_combat_visual(const Render::GL::AnimationInputs& inputs) noexcept
    -> const Render::Creature::CombatVisualResolvedState* {
  return inputs.combat_visual.authoritative ? &inputs.combat_visual : nullptr;
}

auto resolve_pose_intent_for_animation(const AnimationIntent& animation) noexcept
    -> PoseIntent {
  switch (animation.action) {
  case ActionIntent::Dying:
    return PoseIntent::Dying;
  case ActionIntent::Dead:
    return PoseIntent::Dead;
  case ActionIntent::HitReaction:
    return PoseIntent::HitReaction;
  case ActionIntent::MountedCharge:
    return PoseIntent::RidingCharge;
  default:
    break;
  }

  if (!animation.prioritize_action_over_locomotion) {
    switch (animation.locomotion) {
    case Render::Creature::MovementAnimationState::Run:
      return PoseIntent::Run;
    case Render::Creature::MovementAnimationState::Walk:
      return PoseIntent::Walk;
    case Render::Creature::MovementAnimationState::Idle:
      break;
    }
  }

  switch (animation.action) {
  case ActionIntent::AttackMelee:
    return PoseIntent::AttackMelee;
  case ActionIntent::AttackSpear:
    return PoseIntent::AttackSpear;
  case ActionIntent::AttackRanged:
    return PoseIntent::AttackRanged;
  case ActionIntent::Cast:
    return PoseIntent::Cast;
  case ActionIntent::Healing:
    return PoseIntent::Healing;
  case ActionIntent::Construct:
    return PoseIntent::Construct;
  case ActionIntent::None:
  case ActionIntent::MountedCharge:
  case ActionIntent::HitReaction:
  case ActionIntent::Dying:
  case ActionIntent::Dead:
    break;
  }

  if (animation.stance == StanceIntent::Hold ||
      animation.stance == StanceIntent::Guard) {
    return PoseIntent::Hold;
  }

  return PoseIntent::Idle;
}

} // namespace

auto is_stationary_melee_combat_phase(Render::GL::CombatAnimPhase phase) noexcept
    -> bool {
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

auto should_prioritize_attack_pose_over_locomotion(
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

auto is_attack_pose_intent(PoseIntent intent) noexcept -> bool {
  switch (intent) {
  case PoseIntent::AttackMelee:
  case PoseIntent::AttackSpear:
  case PoseIntent::AttackRanged:
  case PoseIntent::Cast:
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

auto is_locomotion_pose_intent(PoseIntent intent) noexcept -> bool {
  switch (intent) {
  case PoseIntent::Walk:
  case PoseIntent::Run:
    return true;
  case PoseIntent::Idle:
  case PoseIntent::Hold:
  case PoseIntent::AttackMelee:
  case PoseIntent::AttackSpear:
  case PoseIntent::AttackRanged:
  case PoseIntent::Cast:
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

auto is_attack_animation_state(AnimationStateId state) noexcept -> bool {
  switch (state) {
  case AnimationStateId::AttackSword:
  case AnimationStateId::AttackSpear:
  case AnimationStateId::AttackBow:
  case AnimationStateId::Cast:
    return true;
  case AnimationStateId::Idle:
  case AnimationStateId::Walk:
  case AnimationStateId::Run:
  case AnimationStateId::Hold:
  case AnimationStateId::AttackMelee:
  case AnimationStateId::AttackRanged:
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

auto resolve_animation_intent(const Render::GL::AnimationInputs& inputs) noexcept
    -> AnimationIntent {
  AnimationIntent intent{};
  intent.locomotion = inputs.movement_state;
  if (auto const* combat = resolved_combat_visual(inputs); combat != nullptr) {
    intent.attack_family = combat->attack_family;
  } else {
    intent.attack_family = inputs.attack_family;
  }

  if (inputs.is_mounted) {
    intent.stance = StanceIntent::Mounted;
  } else if (inputs.is_in_hold_mode || inputs.is_exiting_hold) {
    intent.stance = StanceIntent::Hold;
  } else if ((inputs.is_guarding || inputs.is_exiting_guard) &&
             !Render::Creature::is_moving_animation(inputs.movement_state)) {
    intent.stance = StanceIntent::Guard;
  }

  if (auto const* combat = resolved_combat_visual(inputs); combat != nullptr) {
    intent.prioritize_action_over_locomotion =
        combat->prioritize_action_over_locomotion;
  } else {
    intent.prioritize_action_over_locomotion =
        should_prioritize_attack_pose_over_locomotion(inputs);
  }

  if (inputs.is_dying) {
    intent.action = ActionIntent::Dying;
    return intent;
  }
  if (inputs.is_dead) {
    intent.action = ActionIntent::Dead;
    return intent;
  }
  if (inputs.is_hit_reacting) {
    intent.action = ActionIntent::HitReaction;
    return intent;
  }
  auto const* combat = resolved_combat_visual(inputs);
  bool const combat_attacking =
      (combat != nullptr) ? combat->active : inputs.is_attacking;
  bool const combat_melee = (combat != nullptr) ? combat->is_melee : inputs.is_melee;
  bool const combat_mounted =
      (combat != nullptr) ? combat->is_mounted : inputs.is_mounted;
  bool const combat_casting =
      (combat != nullptr) ? combat->is_casting : inputs.is_casting;
  Engine::Core::CombatAttackFamily const attack_family =
      (combat != nullptr) ? combat->attack_family : inputs.attack_family;

  if (combat_mounted && combat_attacking && combat_melee) {
    intent.prioritize_action_over_locomotion = true;
    switch (attack_family) {
    case Engine::Core::CombatAttackFamily::Sword:
      intent.action = ActionIntent::AttackMelee;
      break;
    case Engine::Core::CombatAttackFamily::Spear:
    case Engine::Core::CombatAttackFamily::None:
    default:
      intent.action = ActionIntent::MountedCharge;
      break;
    }
    return intent;
  }
  if (combat_attacking) {
    if (combat_casting) {
      intent.action = ActionIntent::Cast;
      intent.cast_kind = inputs.cast_kind;
      return intent;
    }
    if (inputs.is_in_hold_mode) {
      intent.action =
          inputs.is_melee ? ActionIntent::AttackMelee : ActionIntent::AttackRanged;
      return intent;
    }
    switch (attack_family) {
    case Engine::Core::CombatAttackFamily::Spear:
      intent.action = ActionIntent::AttackSpear;
      return intent;
    case Engine::Core::CombatAttackFamily::Bow:
      intent.action = ActionIntent::AttackRanged;
      return intent;
    case Engine::Core::CombatAttackFamily::Sword:
      intent.action = ActionIntent::AttackMelee;
      return intent;
    case Engine::Core::CombatAttackFamily::None:
    default:
      intent.action =
          inputs.is_melee ? ActionIntent::AttackMelee : ActionIntent::AttackRanged;
      return intent;
    }
  }
  if (inputs.is_healing) {
    intent.action = ActionIntent::Healing;
    return intent;
  }
  if (inputs.is_constructing) {
    intent.action = ActionIntent::Construct;
    return intent;
  }
  return intent;
}

auto resolve_pose_intent(const Render::GL::AnimationInputs& inputs) noexcept
    -> PoseIntent {
  return resolve_pose_intent_for_animation(resolve_animation_intent(inputs));
}

auto resolve_pose_for_intent(PoseIntent intent) noexcept -> ResolvedPose {
  using A = Render::Creature::AnimationStateId;
  using H = Render::Humanoid::HumanoidState;
  using M = Render::GL::HumanoidMotionState;

  ResolvedPose resolved{};
  resolved.intent = intent;
  switch (intent) {
  case PoseIntent::Walk:
    resolved.motion_state = M::Walk;
    resolved.humanoid_state = H::Walk;
    resolved.animation_state = A::Walk;
    resolved.animation.action = ActionIntent::None;
    resolved.animation.locomotion = Render::Creature::MovementAnimationState::Walk;
    break;
  case PoseIntent::Run:
    resolved.motion_state = M::Run;
    resolved.humanoid_state = H::Run;
    resolved.animation_state = A::Run;
    resolved.animation.action = ActionIntent::None;
    resolved.animation.locomotion = Render::Creature::MovementAnimationState::Run;
    break;
  case PoseIntent::Hold:
    resolved.motion_state = M::Hold;
    resolved.humanoid_state = H::Hold;
    resolved.animation_state = A::Hold;
    resolved.animation.action = ActionIntent::None;
    resolved.animation.stance = StanceIntent::Hold;
    break;
  case PoseIntent::AttackMelee:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackMelee;
    resolved.animation_state = A::AttackSword;
    resolved.animation.action = ActionIntent::AttackMelee;
    break;
  case PoseIntent::AttackSpear:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackMelee;
    resolved.animation_state = A::AttackSpear;
    resolved.animation.action = ActionIntent::AttackSpear;
    break;
  case PoseIntent::AttackRanged:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackRanged;
    resolved.animation_state = A::AttackBow;
    resolved.animation.action = ActionIntent::AttackRanged;
    break;
  case PoseIntent::Cast:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackRanged;
    resolved.animation_state = A::Cast;
    resolved.animation.action = ActionIntent::Cast;
    break;
  case PoseIntent::HitReaction:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::HitReaction;
    resolved.animation_state = A::Idle;
    resolved.animation.action = ActionIntent::HitReaction;
    break;
  case PoseIntent::Healing:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Healing;
    resolved.animation_state = A::Idle;
    resolved.animation.action = ActionIntent::Healing;
    break;
  case PoseIntent::Construct:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Construct;
    resolved.animation_state = A::AttackSword;
    resolved.animation.action = ActionIntent::Construct;
    break;
  case PoseIntent::Dying:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Death;
    resolved.animation_state = A::Die;
    resolved.animation.action = ActionIntent::Dying;
    break;
  case PoseIntent::Dead:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Death;
    resolved.animation_state = A::Dead;
    resolved.animation.action = ActionIntent::Dead;
    break;
  case PoseIntent::RidingIdle:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    resolved.animation_state = A::RidingIdle;
    resolved.animation.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::RidingCharge:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    resolved.animation_state = A::RidingCharge;
    resolved.animation.action = ActionIntent::MountedCharge;
    resolved.animation.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::RidingReining:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    resolved.animation_state = A::RidingReining;
    resolved.animation.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::RidingBowShot:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    resolved.animation_state = A::RidingBowShot;
    resolved.animation.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::Idle:
  case PoseIntent::Count:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    resolved.animation_state = A::Idle;
    break;
  }
  return resolved;
}

auto resolve_pose(const Render::GL::AnimationInputs& inputs) noexcept -> ResolvedPose {
  auto const animation = resolve_animation_intent(inputs);
  auto resolved_pose =
      resolve_pose_for_intent(resolve_pose_intent_for_animation(animation));
  resolved_pose.animation = animation;
  if (inputs.is_attacking && inputs.is_in_hold_mode && !inputs.is_casting) {
    resolved_pose.animation_state = inputs.is_melee ? AnimationStateId::AttackMelee
                                                    : AnimationStateId::AttackRanged;
  }
  if (inputs.is_casting) {
    resolved_pose.animation.cast_kind = inputs.cast_kind;
  }
  return resolved_pose;
}

auto humanoid_motion_state_for_intent(PoseIntent intent) noexcept
    -> Render::GL::HumanoidMotionState {
  return resolve_pose_for_intent(intent).motion_state;
}

auto humanoid_state_for_intent(PoseIntent intent) noexcept
    -> Render::Humanoid::HumanoidState {
  return resolve_pose_for_intent(intent).humanoid_state;
}

auto animation_state_for_intent(PoseIntent intent) noexcept
    -> Render::Creature::AnimationStateId {
  return resolve_pose_for_intent(intent).animation_state;
}

} // namespace Render::Creature
