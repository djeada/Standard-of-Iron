#pragma once

#include <memory>
#include <vector>

#include "render/frame/i_draw_source.h"

namespace Render::GL {

class GroundRenderer;
class TerrainRenderer;
class FrameSubmitter;
class ResourceManager;

class TerrainSurfaceManager {
public:
  TerrainSurfaceManager();
  ~TerrainSurfaceManager();

  void submit(FrameSubmitter& renderer, ResourceManager* resources);

  [[nodiscard]] auto ground() const -> GroundRenderer*;
  [[nodiscard]] auto terrain() const -> TerrainRenderer*;
  [[nodiscard]] auto sources() const -> const std::vector<IDrawSource*>&;

  void set_world_view(const Render::WorldView& view) noexcept {
    for (auto* source : sources()) {
      if (source != nullptr) {
        source->set_world_view(view);
      }
    }
  }

private:
  std::unique_ptr<GroundRenderer> m_ground;
  std::unique_ptr<TerrainRenderer> m_terrain;
  std::vector<IDrawSource*> m_sources;
};

} // namespace Render::GL
