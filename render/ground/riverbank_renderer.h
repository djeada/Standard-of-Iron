#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include <QMatrix4x4>
#include <memory>
#include <vector>

namespace Render::GL {
class Mesh;
class Renderer;
class ResourceManager;

class RiverbankRenderer : public IRenderPass {
public:
  RiverbankRenderer();
  ~RiverbankRenderer() override;

  void configure(const std::vector<Game::Map::RiverSegment> &riverSegments,
                 const Game::Map::TerrainHeightMap &height_map);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void build_meshes();

  std::vector<Game::Map::RiverSegment> m_riverSegments;
  float m_tile_size = 1.0F;
  int m_grid_width = 0;
  int m_grid_height = 0;
  std::vector<float> m_heights;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
  std::vector<std::vector<QVector3D>> m_visibilitySamples;
};

} // namespace Render::GL
