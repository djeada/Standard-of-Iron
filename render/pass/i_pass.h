

#pragma once

#include <string_view>

namespace Render::Pass {

struct FrameContext;

class IFramePass {
public:
  virtual ~IFramePass() = default;

  [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;

  virtual void execute(FrameContext &ctx) = 0;
};

} // namespace Render::Pass
