#pragma once

#include <cstddef>
#include <cstdint>

#include "clip_manifest.h"

namespace Animation {

struct PoseVec3 {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

enum class MovementState : std::uint8_t {
  Idle,
  Walk,
  Run,
};

[[nodiscard]] constexpr auto is_moving(MovementState state) noexcept -> bool {
  return state != MovementState::Idle;
}

[[nodiscard]] constexpr auto is_running(MovementState state) noexcept -> bool {
  return state == MovementState::Run;
}

[[nodiscard]] constexpr auto
state_for_movement(MovementState state) noexcept -> StateId {
  switch (state) {
  case MovementState::Idle:
    return StateId::Idle;
  case MovementState::Walk:
    return StateId::Walk;
  case MovementState::Run:
    return StateId::Run;
  }
  return StateId::Idle;
}

enum class PoseIntent : std::uint8_t {
  Idle = 0,
  Walk,
  Run,
  Hold,

  AttackMelee,
  AttackSpear,
  AttackRanged,
  Cast,
  HitReaction,

  Healing,
  Construct,

  RidingIdle,
  RidingCharge,
  RidingReining,
  RidingBowShot,

  Dying,
  Dead,
  Count
};

[[nodiscard]] constexpr auto pose_intent_count() noexcept -> std::size_t {
  return static_cast<std::size_t>(PoseIntent::Count);
}

enum class ActionIntent : std::uint8_t {
  None = 0,
  AttackMelee,
  AttackSpear,
  AttackRanged,
  Cast,
  MountedCharge,
  HitReaction,
  Healing,
  Construct,
  Dying,
  Dead,
};

enum class StanceIntent : std::uint8_t {
  Normal = 0,
  Hold,
  Guard,
  Mounted,
};

struct SemanticIntent {
  MovementState locomotion{MovementState::Idle};
  ActionIntent action{ActionIntent::None};
  StanceIntent stance{StanceIntent::Normal};
  bool prioritize_action_over_locomotion{false};
};

struct PoseResolution {
  PoseIntent intent{PoseIntent::Idle};
  StateId animation_state{StateId::Idle};
  SemanticIntent semantic{};
};

[[nodiscard]] constexpr auto is_attack_pose_intent(PoseIntent intent) noexcept -> bool {
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

[[nodiscard]] constexpr auto
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

[[nodiscard]] constexpr auto is_attack_state(StateId state) noexcept -> bool {
  switch (state) {
  case StateId::AttackSword:
  case StateId::AttackSpear:
  case StateId::AttackBow:
  case StateId::Cast:
  case StateId::RpgSwordSlashLeft:
  case StateId::RpgSwordSlashRight:
  case StateId::RpgSwordOverhead:
  case StateId::RpgSwordThrust:
  case StateId::RpgSwordFinisher:
    return true;
  case StateId::Idle:
  case StateId::Walk:
  case StateId::Run:
  case StateId::Hold:
  case StateId::AttackMelee:
  case StateId::AttackRanged:
  case StateId::Die:
  case StateId::Dead:
  case StateId::RidingIdle:
  case StateId::RidingCharge:
  case StateId::RidingReining:
  case StateId::RidingBowShot:
  case StateId::Count:
    return false;
  }
  return false;
}

[[nodiscard]] constexpr auto pose_intent_for_semantic_intent(
    const SemanticIntent& animation) noexcept -> PoseIntent {
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
    case MovementState::Run:
      return PoseIntent::Run;
    case MovementState::Walk:
      return PoseIntent::Walk;
    case MovementState::Idle:
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

[[nodiscard]] constexpr auto
resolve_pose(PoseIntent intent) noexcept -> PoseResolution {
  PoseResolution resolved{};
  resolved.intent = intent;
  switch (intent) {
  case PoseIntent::Walk:
    resolved.animation_state = StateId::Walk;
    resolved.semantic.action = ActionIntent::None;
    resolved.semantic.locomotion = MovementState::Walk;
    break;
  case PoseIntent::Run:
    resolved.animation_state = StateId::Run;
    resolved.semantic.action = ActionIntent::None;
    resolved.semantic.locomotion = MovementState::Run;
    break;
  case PoseIntent::Hold:
    resolved.animation_state = StateId::Hold;
    resolved.semantic.action = ActionIntent::None;
    resolved.semantic.stance = StanceIntent::Hold;
    break;
  case PoseIntent::AttackMelee:
    resolved.animation_state = StateId::AttackSword;
    resolved.semantic.action = ActionIntent::AttackMelee;
    break;
  case PoseIntent::AttackSpear:
    resolved.animation_state = StateId::AttackSpear;
    resolved.semantic.action = ActionIntent::AttackSpear;
    break;
  case PoseIntent::AttackRanged:
    resolved.animation_state = StateId::AttackBow;
    resolved.semantic.action = ActionIntent::AttackRanged;
    break;
  case PoseIntent::Cast:
    resolved.animation_state = StateId::Cast;
    resolved.semantic.action = ActionIntent::Cast;
    break;
  case PoseIntent::HitReaction:
    resolved.animation_state = StateId::Idle;
    resolved.semantic.action = ActionIntent::HitReaction;
    break;
  case PoseIntent::Healing:
    resolved.animation_state = StateId::Idle;
    resolved.semantic.action = ActionIntent::Healing;
    break;
  case PoseIntent::Construct:
    resolved.animation_state = StateId::AttackSword;
    resolved.semantic.action = ActionIntent::Construct;
    break;
  case PoseIntent::Dying:
    resolved.animation_state = StateId::Die;
    resolved.semantic.action = ActionIntent::Dying;
    break;
  case PoseIntent::Dead:
    resolved.animation_state = StateId::Dead;
    resolved.semantic.action = ActionIntent::Dead;
    break;
  case PoseIntent::RidingIdle:
    resolved.animation_state = StateId::RidingIdle;
    resolved.semantic.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::RidingCharge:
    resolved.animation_state = StateId::RidingCharge;
    resolved.semantic.action = ActionIntent::MountedCharge;
    resolved.semantic.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::RidingReining:
    resolved.animation_state = StateId::RidingReining;
    resolved.semantic.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::RidingBowShot:
    resolved.animation_state = StateId::RidingBowShot;
    resolved.semantic.stance = StanceIntent::Mounted;
    break;
  case PoseIntent::Idle:
  case PoseIntent::Count:
    resolved.animation_state = StateId::Idle;
    break;
  }
  return resolved;
}

} // namespace Animation
