#include "terrain_feature_manager.h"

#include "../ground/bridge_renderer.h"
#include "../ground/river_renderer.h"
#include "../ground/riverbank_renderer.h"
#include "../ground/road_renderer.h"

namespace Render::GL {

TerrainFeatureManager::TerrainFeatureManager()
    : m_river(std::make_unique<RiverRenderer>()),
      m_road(std::make_unique<RoadRenderer>()),
      m_riverbank(std::make_unique<RiverbankRenderer>()),
      m_bridge(std::make_unique<BridgeRenderer>()),
      m_passes{m_river.get(), m_road.get(), m_riverbank.get(), m_bridge.get()} {
}

TerrainFeatureManager::~TerrainFeatureManager() = default;

void TerrainFeatureManager::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const std::vector<Game::Map::RoadSegment> &road_segments) {
  const auto &river_segments = height_map.getRiverSegments();
  const auto &bridges = height_map.getBridges();
  const float tile_size = height_map.getTileSize();

  m_river->configure(river_segments, tile_size);
  m_road->configure(road_segments, tile_size);
  m_riverbank->configure(river_segments, height_map);
  m_bridge->configure(bridges, tile_size);
}

void TerrainFeatureManager::submit(Renderer &renderer,
                                   ResourceManager *resources) {
  for (auto *pass : m_passes) {
    if (pass != nullptr) {
      pass->submit(renderer, resources);
    }
  }
}

auto TerrainFeatureManager::river() const -> RiverRenderer * {
  return m_river.get();
}

auto TerrainFeatureManager::road() const -> RoadRenderer * {
  return m_road.get();
}

auto TerrainFeatureManager::riverbank() const -> RiverbankRenderer * {
  return m_riverbank.get();
}

auto TerrainFeatureManager::bridge() const -> BridgeRenderer * {
  return m_bridge.get();
}

auto TerrainFeatureManager::passes() const
    -> const std::vector<IRenderPass *> & {
  return m_passes;
}

} // namespace Render::GL
