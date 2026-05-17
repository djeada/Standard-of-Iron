#pragma once

#include <cstdint>

namespace Render::Creature {

enum class MovementAnimationState : std::uint8_t {
  Idle,
  Walk,
  Run,
};

[[nodiscard]] constexpr auto
is_moving_animation(MovementAnimationState state) noexcept -> bool {
  return state != MovementAnimationState::Idle;
}

[[nodiscard]] constexpr auto
is_running_animation(MovementAnimationState state) noexcept -> bool {
  return state == MovementAnimationState::Run;
}

} // namespace Render::Creature
