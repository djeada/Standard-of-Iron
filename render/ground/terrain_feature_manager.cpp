#include "terrain_feature_manager.h"

#include "../ground/bridge_renderer.h"
#include "../ground/river_renderer.h"
#include "../ground/riverbank_renderer.h"
#include "../ground/road_renderer.h"

namespace Render::GL {

TerrainFeatureManager::TerrainFeatureManager()
    : m_water(std::make_unique<WaterRenderer>())
    , m_road(std::make_unique<RoadRenderer>())
    , m_shoreline(std::make_unique<ShorelineRenderer>())
    , m_bridge(std::make_unique<BridgeRenderer>())
    , m_passes{m_water.get(), m_road.get(), m_shoreline.get(), m_bridge.get()} {
}

TerrainFeatureManager::~TerrainFeatureManager() = default;

void TerrainFeatureManager::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const std::vector<Game::Map::RoadSegment>& road_segments) {
  const auto& river_segments = height_map.get_river_segments();
  const auto& lakes = height_map.get_lakes();
  const auto& bridges = height_map.get_bridges();
  const float tile_size = height_map.get_tile_size();

  m_water->configure(river_segments, lakes, height_map);
  m_road->configure(road_segments, height_map);
  m_shoreline->configure(river_segments, lakes, height_map);
  m_bridge->configure(bridges, tile_size);
}

void TerrainFeatureManager::submit(Renderer& renderer, ResourceManager* resources) {
  for (auto* pass : m_passes) {
    if (pass != nullptr) {
      pass->submit(renderer, resources);
    }
  }
}

auto TerrainFeatureManager::water() const -> WaterRenderer* {
  return m_water.get();
}

auto TerrainFeatureManager::road() const -> RoadRenderer* {
  return m_road.get();
}

auto TerrainFeatureManager::shoreline() const -> ShorelineRenderer* {
  return m_shoreline.get();
}

auto TerrainFeatureManager::bridge() const -> BridgeRenderer* {
  return m_bridge.get();
}

auto TerrainFeatureManager::chunks(std::size_t water_count,
                                   std::size_t road_count,
                                   std::size_t bridge_count) const
    -> std::vector<LinearFeatureChunk> {
  return {{LinearFeatureKind::Water,
           LinearFeatureVisibilityMode::SegmentSampled,
           m_water.get(),
           water_count},
          {LinearFeatureKind::Road,
           LinearFeatureVisibilityMode::SegmentSampled,
           m_road.get(),
           road_count},
          {LinearFeatureKind::Shoreline,
           LinearFeatureVisibilityMode::TextureDriven,
           m_shoreline.get(),
           water_count},
          {LinearFeatureKind::Bridge,
           LinearFeatureVisibilityMode::SegmentSampled,
           m_bridge.get(),
           bridge_count}};
}

auto TerrainFeatureManager::passes() const -> const std::vector<IRenderPass*>& {
  return m_passes;
}

} // namespace Render::GL
