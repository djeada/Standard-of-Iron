#pragma once

#include <QMatrix4x4>

#include <memory>
#include <vector>

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "../terrain_scene_types.h"
#include "visibility_texture_helper.h"

namespace Render::GL {
class Mesh;
class Renderer;
class ResourceManager;

class WaterRenderer : public IRenderPass {
public:
  WaterRenderer();
  ~WaterRenderer() override;

  void configure(const std::vector<Game::Map::RiverSegment>& river_segments,
                 const std::vector<Game::Map::Lake>& lakes,
                 const Game::Map::TerrainHeightMap& height_map);

  void submit(Renderer& renderer, ResourceManager* resources) override;

private:
  void build_meshes();

  std::vector<Game::Map::RiverSegment> m_river_segments;
  std::vector<Game::Map::Lake> m_lakes;
  float m_tile_size = 1.0F;
  const Game::Map::TerrainHeightMap* m_height_map = nullptr;
  struct SurfaceMesh {
    std::unique_ptr<Mesh> mesh;
    WaterSurfaceKind kind = WaterSurfaceKind::River;
    QVector3D visibility_start;
    QVector3D visibility_end;
  };
  std::vector<SurfaceMesh> m_meshes;
  Ground::VisibilityTextureHelper m_vis_helper;
};

} // namespace Render::GL
