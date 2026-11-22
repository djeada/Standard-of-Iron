#pragma once

#include "render/frame/world_view.h"

namespace Render::GL {

class FrameSubmitter;
class DrawResources;

struct IDrawSource {
  virtual ~IDrawSource() = default;
  virtual void submit(FrameSubmitter& renderer, const DrawResources* resources) = 0;

  virtual void set_world_view(const Render::WorldView& view) noexcept {
    m_world_view = view;
  }
  [[nodiscard]] auto world() const noexcept -> const Render::WorldView& {
    return m_world_view;
  }

protected:
  Render::WorldView m_world_view;
};

} // namespace Render::GL
