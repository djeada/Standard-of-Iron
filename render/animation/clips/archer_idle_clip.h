// Stage 12 — authored idle sway clip for humanoid archers.
//
// Channel: "torso_sway_x" — a single float in metres added to the
// torso anchor's X coordinate. The clip is a 2-second loop with 5
// keyframes approximating a gentle sinusoid. Units that opt into
// clip-driven idle sway (via AnimationDriver) sample this clip with
// WrapMode::Loop.
//
// Authoring convention: clip factory functions live in headers so
// static-init lifetime is explicit and unit tests can rebuild them
// deterministically. Factory is not constexpr because std::vector
// keyframes are heap-allocated; the header-inlined factory gives the
// caller one-copy semantics.

#pragma once

#include "../clip.h"

namespace Render::Animation::Clips {

[[nodiscard]] inline auto make_archer_idle_sway_x() -> Clip<float> {
  using KF = Keyframe<float>;
  return Clip<float>("archer_idle_sway_x",
                     {
                         KF{0.00F, 0.00F},
                         KF{0.50F, 0.02F},
                         KF{1.00F, 0.00F},
                         KF{1.50F, -0.02F},
                         KF{2.00F, 0.00F},
                     });
}

[[nodiscard]] inline auto make_archer_walk_sway_x() -> Clip<float> {
  using KF = Keyframe<float>;
  // Faster, wider — 1-second loop, 4 keyframes.
  return Clip<float>("archer_walk_sway_x",
                     {
                         KF{0.00F, 0.00F},
                         KF{0.25F, 0.06F},
                         KF{0.50F, 0.00F},
                         KF{0.75F, -0.06F},
                         KF{1.00F, 0.00F},
                     });
}

} // namespace Render::Animation::Clips
