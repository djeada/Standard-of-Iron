#include "pose_intent.h"

#include "animation/intent_manifest.h"
#include "animation_core_bridge.h"

namespace Render::Creature {

namespace {

auto resolved_combat_visual(const Render::GL::AnimationInputs& inputs) noexcept
    -> const Render::Creature::CombatVisualResolvedState* {
  return inputs.combat_visual.authoritative ? &inputs.combat_visual : nullptr;
}

auto semantic_intent(const AnimationIntent& animation) noexcept
    -> Animation::SemanticIntent {
  return {
      .locomotion = animation.locomotion,
      .action = animation.action,
      .stance = animation.stance,
      .prioritize_action_over_locomotion = animation.prioritize_action_over_locomotion,
  };
}

auto intent_inputs(const Render::GL::AnimationInputs& inputs) noexcept
    -> Animation::HumanoidIntentInputs {
  auto const* combat = resolved_combat_visual(inputs);
  return {
      .locomotion = inputs.movement_state,
      .is_mounted = inputs.is_mounted,
      .is_in_hold_mode = inputs.is_in_hold_mode,
      .is_exiting_hold = inputs.is_exiting_hold,
      .is_guarding = inputs.is_guarding,
      .is_exiting_guard = inputs.is_exiting_guard,
      .has_chase_intent = inputs.visual_movement.has_chase_intent,
      .is_dying = inputs.is_dying,
      .is_dead = inputs.is_dead,
      .is_hit_reacting = inputs.is_hit_reacting,
      .is_healing = inputs.is_healing,
      .is_constructing = inputs.is_constructing,
      .is_attacking = inputs.is_attacking,
      .is_melee = inputs.is_melee,
      .is_in_melee_lock = inputs.is_in_melee_lock,
      .is_casting = inputs.is_casting,
      .combat_phase = animation_combat_phase(inputs.combat_phase),
      .attack_family = animation_attack_family(inputs.attack_family),
      .has_authoritative_combat = combat != nullptr,
      .combat_active = combat != nullptr ? combat->active : false,
      .combat_is_melee = combat != nullptr ? combat->is_melee : false,
      .combat_is_mounted = combat != nullptr ? combat->is_mounted : false,
      .combat_is_casting = combat != nullptr ? combat->is_casting : false,
      .combat_prioritize_action_over_locomotion =
          combat != nullptr ? combat->prioritize_action_over_locomotion : false,
      .combat_attack_family = combat != nullptr ? combat->attack_family
                                                : Animation::CombatAttackFamily::None,
  };
}

} // namespace

auto is_stationary_melee_combat_phase(Render::GL::CombatAnimPhase phase) noexcept
    -> bool {
  return Animation::is_stationary_melee_combat_phase(animation_combat_phase(phase));
}

auto should_prioritize_attack_pose_over_locomotion(
    const Render::GL::AnimationInputs& inputs) noexcept -> bool {
  return Animation::should_prioritize_humanoid_attack_pose(intent_inputs(inputs));
}

auto is_attack_pose_intent(PoseIntent intent) noexcept -> bool {
  return Animation::is_attack_pose_intent(intent);
}

auto is_locomotion_pose_intent(PoseIntent intent) noexcept -> bool {
  return Animation::is_locomotion_pose_intent(intent);
}

auto is_attack_animation_state(AnimationStateId state) noexcept -> bool {
  return Animation::is_attack_state(state);
}

auto resolve_animation_intent(const Render::GL::AnimationInputs& inputs) noexcept
    -> AnimationIntent {
  auto const resolution = Animation::resolve_humanoid_intent(intent_inputs(inputs));
  AnimationIntent intent{};
  intent.locomotion = resolution.semantic.locomotion;
  intent.action = resolution.semantic.action;
  intent.stance = resolution.semantic.stance;
  intent.prioritize_action_over_locomotion =
      resolution.semantic.prioritize_action_over_locomotion;
  intent.attack_family = engine_attack_family(resolution.attack_family);
  if (inputs.is_casting) {
    intent.cast_kind = inputs.cast_kind;
  }
  return intent;
}

auto resolve_pose_intent(const Render::GL::AnimationInputs& inputs) noexcept
    -> PoseIntent {
  return Animation::pose_intent_for_semantic_intent(
      semantic_intent(resolve_animation_intent(inputs)));
}

auto resolve_pose_for_intent(PoseIntent intent) noexcept -> ResolvedPose {
  using H = Render::Humanoid::HumanoidState;
  using M = Render::GL::HumanoidMotionState;

  ResolvedPose resolved{};
  auto const semantic = Animation::resolve_pose(intent);
  resolved.intent = intent;
  resolved.animation_state = semantic.animation_state;
  resolved.animation.locomotion = semantic.semantic.locomotion;
  resolved.animation.action = semantic.semantic.action;
  resolved.animation.stance = semantic.semantic.stance;
  switch (intent) {
  case PoseIntent::Walk:
    resolved.motion_state = M::Walk;
    resolved.humanoid_state = H::Walk;
    break;
  case PoseIntent::Run:
    resolved.motion_state = M::Run;
    resolved.humanoid_state = H::Run;
    break;
  case PoseIntent::Hold:
    resolved.motion_state = M::Hold;
    resolved.humanoid_state = H::Hold;
    break;
  case PoseIntent::AttackMelee:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackMelee;
    break;
  case PoseIntent::AttackSpear:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackMelee;
    break;
  case PoseIntent::AttackRanged:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackRanged;
    break;
  case PoseIntent::Cast:
    resolved.motion_state = M::Attacking;
    resolved.humanoid_state = H::AttackRanged;
    break;
  case PoseIntent::HitReaction:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::HitReaction;
    break;
  case PoseIntent::Healing:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Healing;
    break;
  case PoseIntent::Construct:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Construct;
    break;
  case PoseIntent::Dying:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Death;
    break;
  case PoseIntent::Dead:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Death;
    break;
  case PoseIntent::RidingIdle:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    break;
  case PoseIntent::RidingCharge:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    break;
  case PoseIntent::RidingReining:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    break;
  case PoseIntent::RidingBowShot:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    break;
  case PoseIntent::Idle:
  case PoseIntent::Count:
    resolved.motion_state = M::Idle;
    resolved.humanoid_state = H::Idle;
    break;
  }
  return resolved;
}

auto resolve_pose(const Render::GL::AnimationInputs& inputs) noexcept -> ResolvedPose {
  auto const animation = resolve_animation_intent(inputs);
  auto resolved_pose = resolve_pose_for_intent(
      Animation::pose_intent_for_semantic_intent(semantic_intent(animation)));
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
