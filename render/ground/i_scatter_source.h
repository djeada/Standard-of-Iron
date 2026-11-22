#pragma once

#include <QVector3D>

#include <cstddef>

#include "render/frame/i_draw_source.h"
#include "render/ground/scatter_renderer_state.h"

namespace Render::GL {

struct IScatterSource : IDrawSource {
  [[nodiscard]] virtual auto is_gpu_ready() const -> bool = 0;
  [[nodiscard]] virtual auto instance_count() const -> std::size_t = 0;
  // CPU bytes held by the placed instances.
  [[nodiscard]] virtual auto instance_bytes() const -> std::size_t = 0;
  [[nodiscard]] virtual auto
  last_sync_stats() const -> Render::Ground::Scatter::SyncStats = 0;

  virtual void clear() = 0;

  // Load time: partition the instances and reserve their instance blocks, so
  // a backend can create the buffers before the first frame.
  virtual void prepare_instance_blocks() {}
  virtual void for_each_instance_block(const InstanceBlockVisitor& visit) const {
    (void)visit;
  }

  virtual void set_light_direction(const QVector3D& dir) { (void)dir; }
};

} // namespace Render::GL
