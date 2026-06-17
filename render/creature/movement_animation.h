#pragma once

#include "movement_state.h"
#include "render_request.h"

namespace Render::Creature {

[[nodiscard]] constexpr auto animation_state_for_movement(
    MovementAnimationState state) noexcept -> AnimationStateId {
  return Animation::state_for_movement(state);
}

} // namespace Render::Creature
