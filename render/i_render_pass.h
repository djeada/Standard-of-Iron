#pragma once

#include "render/world_view.h"

namespace Render::GL {

class Renderer;
class ResourceManager;

struct IRenderPass {
  virtual ~IRenderPass() = default;
  virtual void submit(Renderer& renderer, ResourceManager* resources) = 0;

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
