#pragma once

#include <cstdint>

#include "render/humanoid/runtime/runtime_stats.h"

namespace Render::Humanoid {

struct HumanoidRuntimeContext {

  std::uint32_t frame_index{0U};
  Render::GL::HumanoidRenderStats stats;

  void begin_frame() noexcept { ++frame_index; }
  void reset_stats() noexcept { stats.reset(); }
  void reset() noexcept {
    frame_index = 0U;
    stats.reset();
  }
};

class ScopedHumanoidRuntimeContext {
public:
  explicit ScopedHumanoidRuntimeContext(HumanoidRuntimeContext& context) noexcept;
  ~ScopedHumanoidRuntimeContext();
  ScopedHumanoidRuntimeContext(const ScopedHumanoidRuntimeContext&) = delete;
  auto operator=(const ScopedHumanoidRuntimeContext&) -> ScopedHumanoidRuntimeContext& =
                                                             delete;

private:
  HumanoidRuntimeContext* m_previous{nullptr};
};

[[nodiscard]] auto
current_humanoid_runtime_context() noexcept -> HumanoidRuntimeContext&;

} // namespace Render::Humanoid
