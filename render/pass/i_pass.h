// Stage 10 — frame-pass abstraction.
//
// A `FramePass` is one cohesive step of per-frame rendering: collect
// visible entities, classify them by tier, submit them to the queue,
// flush a primitive batch, render construction previews, etc.
//
// Design intent:
//   * Each pass is a small stateful object (or stateless free-function
//     wrapped in a struct) with a single `execute(FrameContext&)` entry
//     point. No virtual tree beyond this one-method interface.
//   * Passes don't own state between frames — long-lived state lives on
//     the Renderer or Backend. The pass is just logic with a name.
//   * `name()` is used by a future profiling HUD (Stage 14) to attribute
//     GPU/CPU time to passes. Today it's also just helpful for asserts.
//   * Passes compose through `FramePassRunner` (see
//     frame_pass_runner.h) so `render_world()` becomes an orchestrator,
//     not a kitchen sink.

#pragma once

#include <string_view>

namespace Render::Pass {

struct FrameContext;

class IFramePass {
public:
  virtual ~IFramePass() = default;

  // Human-readable name for profiling / logging. Must be stable across
  // the pass's lifetime (typical implementation returns a string literal).
  [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;

  // Do the work. The pass may read and write ctx freely; ordering is the
  // runner's responsibility.
  virtual void execute(FrameContext &ctx) = 0;
};

} // namespace Render::Pass
