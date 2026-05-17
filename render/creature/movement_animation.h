#pragma once

#include "movement_state.h"
#include "render_request.h"

namespace Render::Creature {

[[nodiscard]] constexpr auto animation_state_for_movement(
    MovementAnimationState state) noexcept -> AnimationStateId {
  switch (state) {
  case MovementAnimationState::Idle:
    return AnimationStateId::Idle;
  case MovementAnimationState::Walk:
    return AnimationStateId::Walk;
  case MovementAnimationState::Run:
    return AnimationStateId::Run;
  }
  return AnimationStateId::Idle;
}

} // namespace Render::Creature
