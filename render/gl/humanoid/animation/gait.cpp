#include "gait.h"

#include "../humanoid_constants.h"

namespace Render::GL {

auto classify_motion_state(const AnimationInputs &anim,
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
  if (anim.is_moving) {

    if (anim.is_running) {
      return HumanoidMotionState::Run;
    }
    return (move_speed > k_run_speed_threshold) ? HumanoidMotionState::Run
                                                : HumanoidMotionState::Walk;
  }
  return HumanoidMotionState::Idle;
}

} // namespace Render::GL
