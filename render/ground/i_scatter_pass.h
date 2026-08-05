#pragma once

#include <QVector3D>

#include <cstddef>

#include "../i_render_pass.h"
#include "scatter_renderer_state.h"

namespace Render::GL {

struct IScatterPass : IRenderPass {
  [[nodiscard]] virtual auto is_gpu_ready() const -> bool = 0;
  [[nodiscard]] virtual auto instance_count() const -> std::size_t = 0;
  [[nodiscard]] virtual auto
  last_sync_stats() const -> Render::Ground::Scatter::SyncStats = 0;

  virtual void clear() = 0;

  virtual void set_light_direction(const QVector3D& dir) { (void)dir; }
};

} // namespace Render::GL
