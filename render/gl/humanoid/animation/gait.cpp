#include "gait.h"

#include "../../../creature/movement_animation.h"
#include "../humanoid_constants.h"

namespace Render::GL {

auto classify_motion_state(const AnimationInputs& anim,
                           float move_speed) -> HumanoidMotionState {
  if (anim.is_in_hold_mode) {
    return HumanoidMotionState::Hold;
  }
  if (anim.is_exiting_hold) {
    return HumanoidMotionState::ExitingHold;
  }
  if (anim.is_attacking) {
    return HumanoidMotionState::Attacking;
  }
  switch (Render::Creature::movement_animation_from_speed(
      anim.is_moving, anim.is_running, move_speed, k_run_speed_threshold)) {
  case Render::Creature::MovementAnimationState::Run:
    return HumanoidMotionState::Run;
  case Render::Creature::MovementAnimationState::Walk:
    return HumanoidMotionState::Walk;
  case Render::Creature::MovementAnimationState::Idle:
    return HumanoidMotionState::Idle;
  }
  return HumanoidMotionState::Idle;
}

} // namespace Render::GL
