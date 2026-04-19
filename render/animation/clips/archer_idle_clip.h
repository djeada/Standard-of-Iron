

#pragma once

#include "../clip.h"

namespace Render::Animation::Clips {

[[nodiscard]] inline auto make_archer_idle_sway_x() -> Clip<float> {
  using KF = Keyframe<float>;
  return Clip<float>("archer_idle_sway_x", {
                                               KF{0.00F, 0.00F},
                                               KF{0.50F, 0.02F},
                                               KF{1.00F, 0.00F},
                                               KF{1.50F, -0.02F},
                                               KF{2.00F, 0.00F},
                                           });
}

[[nodiscard]] inline auto make_archer_walk_sway_x() -> Clip<float> {
  using KF = Keyframe<float>;

  return Clip<float>("archer_walk_sway_x", {
                                               KF{0.00F, 0.00F},
                                               KF{0.25F, 0.06F},
                                               KF{0.50F, 0.00F},
                                               KF{0.75F, -0.06F},
                                               KF{1.00F, 0.00F},
                                           });
}

} // namespace Render::Animation::Clips
