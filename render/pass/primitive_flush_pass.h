// Stage 10 — flush the Primitive batcher into the active DrawQueue.
//
// Extracted from render_world phase 6. Emits up to three
// PrimitiveBatchCmds (sphere / cylinder / cone) as a single ordered
// step so downstream passes don't observe a half-flushed batcher.

#pragma once

#include "i_pass.h"

namespace Render::Pass {

class PrimitiveFlushPass final : public IFramePass {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "primitive_flush";
  }
  void execute(FrameContext &ctx) override;
};

} // namespace Render::Pass
