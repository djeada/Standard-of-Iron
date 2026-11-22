#pragma once

#include <QMatrix4x4>

#include <memory>
#include <vector>

#include "game/map/terrain.h"
#include "render/frame/i_draw_source.h"
#include "render/gl/mesh_gpu_table.h"
#include "render/ground/road_network_geometry.h"

namespace Render::GL {
class Mesh;
class FrameSubmitter;
class ResourceManager;

class RoadRenderer : public IDrawSource {
public:
  RoadRenderer();
  ~RoadRenderer() override;

  void configure(const std::vector<Game::Map::RoadSegment>& road_segments,
                 const Game::Map::TerrainHeightMap& height_map);

  void submit(FrameSubmitter& renderer, ResourceManager* resources) override;

  [[nodiscard]] auto prewarm_gpu_resources(MeshGpuTable& meshes) -> bool;

  [[nodiscard]] auto surface_count() const -> std::size_t { return m_surfaces.size(); }

private:
  void build_meshes();

  std::vector<Game::Map::RoadSegment> m_road_segments;
  const Game::Map::TerrainHeightMap* m_height_map = nullptr;
  float m_tile_size = 1.0F;
  std::vector<Ground::RoadNetworkSurface> m_surfaces;
};

} // namespace Render::GL
