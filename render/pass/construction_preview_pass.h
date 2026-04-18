// Stage 10 — delegates to Renderer::render_construction_previews.
//
// The body of the preview pass is still on Renderer (it touches a lot of
// private render helpers). This pass is a thin adapter so the pass
// runner becomes the single orchestrator for render_world, with no
// "and then call one more method" suffix left dangling.

#pragma once

#include "i_pass.h"

namespace Render::Pass {

class ConstructionPreviewPass final : public IFramePass {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "construction_previews";
  }
  void execute(FrameContext &ctx) override;
};

} // namespace Render::Pass
