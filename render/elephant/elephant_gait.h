#pragma once

#include "animation/rig/quadruped_gait.h"

namespace Render::GL {

struct ElephantGait : Render::Creature::Quadruped::Gait {
  constexpr ElephantGait() noexcept
      : Render::Creature::Quadruped::Gait{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F} {}

  constexpr ElephantGait(float cycle,
                         float front_phase,
                         float rear_phase,
                         float swing,
                         float lift) noexcept
      : Render::Creature::Quadruped::Gait{
            cycle, swing, lift, front_phase, rear_phase, 0.0F} {}
};

} // namespace Render::GL
