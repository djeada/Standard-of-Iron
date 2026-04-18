// Stage 10 — ordered list of frame passes.
//
// Owns a vector of IFramePass pointers and executes them in order. The
// order IS the rendering contract: pass N may depend on any side-effect
// produced by passes 0..N-1.
//
// Keeping the runner separate from the Renderer lets tests wire up a
// fake pass sequence without spinning a GL context.

#pragma once

#include "i_pass.h"

#include <memory>
#include <string_view>
#include <vector>

namespace Render::Pass {

struct FrameContext;

class FramePassRunner {
public:
  FramePassRunner() = default;
  ~FramePassRunner() = default;

  FramePassRunner(const FramePassRunner &) = delete;
  auto operator=(const FramePassRunner &) -> FramePassRunner & = delete;
  FramePassRunner(FramePassRunner &&) noexcept = default;
  auto operator=(FramePassRunner &&) noexcept -> FramePassRunner & = default;

  void add(std::unique_ptr<IFramePass> pass);

  // Append a pass by reference — runner does not own lifetime. Useful for
  // stack-allocated test passes.
  void add_non_owning(IFramePass &pass);

  // Run every pass in submission order. Does not short-circuit on errors:
  // passes are expected to guard their own preconditions.
  void execute(FrameContext &ctx);

  [[nodiscard]] auto size() const noexcept -> std::size_t;
  [[nodiscard]] auto pass_name(std::size_t i) const noexcept
      -> std::string_view;

  void clear() noexcept;

private:
  struct Slot {
    std::unique_ptr<IFramePass> owned;
    IFramePass *ref{nullptr};
  };
  std::vector<Slot> m_slots;
};

} // namespace Render::Pass
