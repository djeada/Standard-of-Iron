#include "render/ground/terrain_surface_manager.h"

#include "render/ground/ground_renderer.h"
#include "render/ground/terrain_renderer.h"

namespace Render::GL {

TerrainSurfaceManager::TerrainSurfaceManager()
    : m_ground(std::make_unique<GroundRenderer>())
    , m_terrain(std::make_unique<TerrainRenderer>())
    , m_sources{m_ground.get(), m_terrain.get()} {
}

TerrainSurfaceManager::~TerrainSurfaceManager() = default;

void TerrainSurfaceManager::submit(FrameSubmitter& renderer,
                                   ResourceManager* resources) {
  for (auto* source : m_sources) {
    if (source != nullptr) {
      source->submit(renderer, resources);
    }
  }
}

auto TerrainSurfaceManager::ground() const -> GroundRenderer* {
  return m_ground.get();
}

auto TerrainSurfaceManager::terrain() const -> TerrainRenderer* {
  return m_terrain.get();
}

auto TerrainSurfaceManager::sources() const -> const std::vector<IDrawSource*>& {
  return m_sources;
}

} // namespace Render::GL
