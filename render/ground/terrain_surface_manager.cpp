#include "terrain_surface_manager.h"

#include "ground_renderer.h"
#include "terrain_renderer.h"

namespace Render::GL {

TerrainSurfaceManager::TerrainSurfaceManager()
    : m_ground(std::make_unique<GroundRenderer>()),
      m_terrain(std::make_unique<TerrainRenderer>()),
      m_passes{m_ground.get(), m_terrain.get()} {}

TerrainSurfaceManager::~TerrainSurfaceManager() = default;

void TerrainSurfaceManager::submit(Renderer &renderer,
                                   ResourceManager *resources) {
  for (auto *pass : m_passes) {
    if (pass != nullptr) {
      pass->submit(renderer, resources);
    }
  }
}

auto TerrainSurfaceManager::ground() const -> GroundRenderer * {
  return m_ground.get();
}

auto TerrainSurfaceManager::terrain() const -> TerrainRenderer * {
  return m_terrain.get();
}

auto TerrainSurfaceManager::passes() const
    -> const std::vector<IRenderPass *> & {
  return m_passes;
}

} // namespace Render::GL
