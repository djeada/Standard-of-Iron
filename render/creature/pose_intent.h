#pragma once

#include "../gl/humanoid/humanoid_types.h"
#include "pose_intent_enum.h"
#include "render_request.h"

namespace Render::Humanoid {

enum class HumanoidState {
  Idle,
  Walk,
  Run,
  Hold,
  AttackMelee,
  AttackRanged,
  HitReaction,
  Healing,
  Construct,
  Death,
};

} // namespace Render::Humanoid

namespace Render::Creature {

using ActionIntent = Animation::ActionIntent;
using StanceIntent = Animation::StanceIntent;

struct AnimationIntent {
  Render::Creature::MovementAnimationState locomotion{
      Render::Creature::MovementAnimationState::Idle};
  ActionIntent action{ActionIntent::None};
  StanceIntent stance{StanceIntent::Normal};
  Render::GL::CastVisualKind cast_kind{Render::GL::CastVisualKind::None};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
  bool prioritize_action_over_locomotion{false};
};

struct ResolvedPose {
  AnimationIntent animation{};
  PoseIntent intent{PoseIntent::Idle};
  Render::GL::HumanoidMotionState motion_state{Render::GL::HumanoidMotionState::Idle};
  Render::Humanoid::HumanoidState humanoid_state{Render::Humanoid::HumanoidState::Idle};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
};

[[nodiscard]] auto
is_stationary_melee_combat_phase(Render::GL::CombatAnimPhase phase) noexcept -> bool;

[[nodiscard]] auto should_prioritize_attack_pose_over_locomotion(
    const Render::GL::AnimationInputs& inputs) noexcept -> bool;

[[nodiscard]] auto is_attack_pose_intent(PoseIntent intent) noexcept -> bool;

[[nodiscard]] auto is_locomotion_pose_intent(PoseIntent intent) noexcept -> bool;

[[nodiscard]] auto is_attack_animation_state(AnimationStateId state) noexcept -> bool;

[[nodiscard]] auto resolve_animation_intent(
    const Render::GL::AnimationInputs& inputs) noexcept -> AnimationIntent;

[[nodiscard]] auto
resolve_pose_intent(const Render::GL::AnimationInputs& inputs) noexcept -> PoseIntent;

[[nodiscard]] auto resolve_pose_for_intent(PoseIntent intent) noexcept -> ResolvedPose;

[[nodiscard]] auto
resolve_pose(const Render::GL::AnimationInputs& inputs) noexcept -> ResolvedPose;

[[nodiscard]] auto humanoid_motion_state_for_intent(PoseIntent intent) noexcept
    -> Render::GL::HumanoidMotionState;

[[nodiscard]] auto humanoid_state_for_intent(PoseIntent intent) noexcept
    -> Render::Humanoid::HumanoidState;

[[nodiscard]] auto animation_state_for_intent(PoseIntent intent) noexcept
    -> Render::Creature::AnimationStateId;

} // namespace Render::Creature
