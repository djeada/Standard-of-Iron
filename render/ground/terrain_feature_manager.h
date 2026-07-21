#pragma once

#include <memory>
#include <vector>

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "../terrain_scene_types.h"

namespace Render::GL {

class BridgeRenderer;
class WaterRenderer;
class ShorelineRenderer;
class RoadRenderer;

class TerrainFeatureManager : public IRenderPass {
public:
  TerrainFeatureManager();
  ~TerrainFeatureManager() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const std::vector<Game::Map::RoadSegment>& road_segments);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  [[nodiscard]] auto water() const -> WaterRenderer*;
  [[nodiscard]] auto road() const -> RoadRenderer*;
  [[nodiscard]] auto shoreline() const -> ShorelineRenderer*;
  [[nodiscard]] auto bridge() const -> BridgeRenderer*;
  [[nodiscard]] auto
  chunks(std::size_t water_count,
         std::size_t road_count,
         std::size_t bridge_count) const -> std::vector<LinearFeatureChunk>;
  [[nodiscard]] auto passes() const -> const std::vector<IRenderPass*>&;

private:
  std::unique_ptr<WaterRenderer> m_water;
  std::unique_ptr<RoadRenderer> m_road;
  std::unique_ptr<ShorelineRenderer> m_shoreline;
  std::unique_ptr<BridgeRenderer> m_bridge;
  std::vector<IRenderPass*> m_passes;
};

} // namespace Render::GL
