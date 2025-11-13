#pragma once

#include "../humanoid_types.h"

namespace Render::GL {

auto classifyMotionState(const AnimationInputs &anim,
                         float move_speed) -> HumanoidMotionState;

} // namespace Render::GL
