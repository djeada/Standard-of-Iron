

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
