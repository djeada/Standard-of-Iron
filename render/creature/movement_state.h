#pragma once

#include "animation/pose_manifest.h"

namespace Render::Creature {

using MovementAnimationState = Animation::MovementState;

[[nodiscard]] constexpr auto
is_moving_animation(MovementAnimationState state) noexcept -> bool {
  return Animation::is_moving(state);
}

[[nodiscard]] constexpr auto
is_running_animation(MovementAnimationState state) noexcept -> bool {
  return Animation::is_running(state);
}

} // namespace Render::Creature
