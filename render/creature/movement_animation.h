#pragma once

#include <cstdint>

#include "render_request.h"

namespace Render::Creature {

enum class MovementAnimationState : std::uint8_t {
  Idle,
  Walk,
  Run,
};

[[nodiscard]] constexpr auto
movement_animation_from_flags(bool is_moving,
                              bool is_running) noexcept -> MovementAnimationState {
  if (!is_moving) {
    return MovementAnimationState::Idle;
  }
  return is_running ? MovementAnimationState::Run : MovementAnimationState::Walk;
}

[[nodiscard]] constexpr auto
movement_animation_from_speed(bool is_moving,
                              bool is_running,
                              float speed,
                              float run_threshold) noexcept -> MovementAnimationState {
  if (!is_moving) {
    return MovementAnimationState::Idle;
  }
  return (is_running || speed > run_threshold) ? MovementAnimationState::Run
                                               : MovementAnimationState::Walk;
}

[[nodiscard]] constexpr auto
is_moving_animation(MovementAnimationState state) noexcept -> bool {
  return state != MovementAnimationState::Idle;
}

[[nodiscard]] constexpr auto
is_running_animation(MovementAnimationState state) noexcept -> bool {
  return state == MovementAnimationState::Run;
}

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
