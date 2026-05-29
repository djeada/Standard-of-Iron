#pragma once

#include "../creature/quadruped/gait.h"

namespace Render::GL {

struct ElephantGait : Render::Creature::Quadruped::Gait {
  constexpr ElephantGait() noexcept
      : Render::Creature::Quadruped::Gait{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F} {}

  constexpr ElephantGait(float cycle_time,
                         float front_leg_phase,
                         float rear_leg_phase,
                         float stride_swing,
                         float stride_lift) noexcept
      : Render::Creature::Quadruped::Gait{cycle_time,
                                          stride_swing,
                                          stride_lift,
                                          front_leg_phase,
                                          rear_leg_phase,
                                          0.0F} {}
};

} // namespace Render::GL
