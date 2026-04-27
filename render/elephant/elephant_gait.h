#pragma once

namespace Render::GL {

struct ElephantGait {
  float cycle_time{};
  float front_leg_phase{};
  float rear_leg_phase{};
  float stride_swing{};
  float stride_lift{};
};

} // namespace Render::GL
