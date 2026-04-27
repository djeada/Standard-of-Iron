#pragma once

#include "../i_render_pass.h"
#include <memory>
#include <vector>

namespace Render::GL {

class GroundRenderer;
class TerrainRenderer;
class Renderer;
class ResourceManager;

class TerrainSurfaceManager {
public:
  TerrainSurfaceManager();
  ~TerrainSurfaceManager();

  void submit(Renderer &renderer, ResourceManager *resources);

  [[nodiscard]] auto ground() const -> GroundRenderer *;
  [[nodiscard]] auto terrain() const -> TerrainRenderer *;
  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass *> &;

private:
  std::unique_ptr<GroundRenderer> m_ground;
  std::unique_ptr<TerrainRenderer> m_terrain;
  std::vector<IRenderPass *> m_passes;
};

} // namespace Render::GL
