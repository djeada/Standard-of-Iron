#pragma once

#include <QMatrix4x4>

#include <memory>
#include <vector>

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "road_network_geometry.h"

namespace Render::GL {
class Mesh;
class Renderer;
class ResourceManager;

class RoadRenderer : public IRenderPass {
public:
  RoadRenderer();
  ~RoadRenderer() override;

  void configure(const std::vector<Game::Map::RoadSegment>& road_segments,
                 const Game::Map::TerrainHeightMap& height_map);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  [[nodiscard]] auto surface_count() const -> std::size_t { return m_surfaces.size(); }

private:
  void build_meshes();

  std::vector<Game::Map::RoadSegment> m_road_segments;
  const Game::Map::TerrainHeightMap* m_height_map = nullptr;
  float m_tile_size = 1.0F;
  std::vector<Ground::RoadNetworkSurface> m_surfaces;
};

} // namespace Render::GL
