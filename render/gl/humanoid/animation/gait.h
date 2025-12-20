#pragma once

#include "../humanoid_types.h"

namespace Render::GL {

auto classify_motion_state(const AnimationInputs &anim,
                           float move_speed) -> HumanoidMotionState;

}
