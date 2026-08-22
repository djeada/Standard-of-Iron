#pragma once

#include "render/creature/quadruped/render_stats.h"

namespace Render::Creature::Quadruped {

struct QuadrupedRuntimeContext {
  Render::GL::QuadrupedRenderStats horse;
  Render::GL::QuadrupedRenderStats elephant;

  void reset() noexcept {
    horse.reset();
    elephant.reset();
  }
};

class ScopedQuadrupedRuntimeContext {
public:
  explicit ScopedQuadrupedRuntimeContext(QuadrupedRuntimeContext& context) noexcept;
  ~ScopedQuadrupedRuntimeContext();
  ScopedQuadrupedRuntimeContext(const ScopedQuadrupedRuntimeContext&) = delete;
  auto operator=(const ScopedQuadrupedRuntimeContext&)
      -> ScopedQuadrupedRuntimeContext& = delete;

private:
  QuadrupedRuntimeContext* m_previous{nullptr};
};

[[nodiscard]] auto
current_quadruped_runtime_context() noexcept -> QuadrupedRuntimeContext&;

} // namespace Render::Creature::Quadruped
