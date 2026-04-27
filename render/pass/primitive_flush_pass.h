

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
