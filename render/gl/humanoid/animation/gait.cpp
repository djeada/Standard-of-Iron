#include "gait.h"

#include "../humanoid_constants.h"

namespace Render::GL {

auto classifyMotionState(const AnimationInputs &anim,
                         float move_speed) -> HumanoidMotionState {
  if (anim.isInHoldMode) {
    return HumanoidMotionState::Hold;
  }
  if (anim.isExitingHold) {
    return HumanoidMotionState::ExitingHold;
  }
  if (anim.is_attacking) {
    return HumanoidMotionState::Attacking;
  }
  if (anim.isMoving) {
    return (move_speed > k_run_speed_threshold) ? HumanoidMotionState::Run
                                                : HumanoidMotionState::Walk;
  }
  return HumanoidMotionState::Idle;
}

} // namespace Render::GL
