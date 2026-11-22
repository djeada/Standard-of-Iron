#pragma once

#include <memory>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/terrain.h"
#include "render/frame/i_draw_source.h"
#include "render/frame/terrain_scene_types.h"
#include "render/geom/mesh.h"

namespace Render::GL {

class BridgeRenderer;
class FogRenderer;
class WaterRenderer;
class ShorelineRenderer;
class RoadRenderer;

class TerrainFeatureManager : public IDrawSource {
public:
  TerrainFeatureManager();
  ~TerrainFeatureManager() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const std::vector<Game::Map::RoadSegment>& road_segments,
                 const Game::Map::BiomeSettings& biome_settings);

  void set_fog_renderer(FogRenderer* fog);

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

  // Every water, road, shoreline and bridge mesh, for prewarming.
  void for_each_mesh(const MeshVisitor& visit) const;

  [[nodiscard]] auto water() const -> WaterRenderer*;
  [[nodiscard]] auto road() const -> RoadRenderer*;
  [[nodiscard]] auto shoreline() const -> ShorelineRenderer*;
  [[nodiscard]] auto bridge() const -> BridgeRenderer*;
  [[nodiscard]] auto
  chunks(std::size_t water_count,
         std::size_t road_count,
         std::size_t bridge_count) const -> std::vector<LinearFeatureChunk>;
  [[nodiscard]] auto sources() const -> const std::vector<IDrawSource*>&;

  void set_world_view(const Render::WorldView& view) noexcept override {
    IDrawSource::set_world_view(view);
    for (auto* source : sources()) {
      if (source != nullptr) {
        source->set_world_view(view);
      }
    }
  }

private:
  std::unique_ptr<WaterRenderer> m_water;
  std::unique_ptr<RoadRenderer> m_road;
  std::unique_ptr<ShorelineRenderer> m_shoreline;
  std::unique_ptr<BridgeRenderer> m_bridge;
  std::vector<IDrawSource*> m_sources;
};

} // namespace Render::GL
