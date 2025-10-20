#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include <QMatrix4x4>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Mesh;
class Renderer;
class ResourceManager;

class RiverbankRenderer : public IRenderPass {
public:
  RiverbankRenderer();
  ~RiverbankRenderer();

  void configure(const std::vector<Game::Map::RiverSegment> &riverSegments,
                 const Game::Map::TerrainHeightMap &heightMap);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void buildMeshes();

  std::vector<Game::Map::RiverSegment> m_riverSegments;
  float m_tileSize = 1.0f;
  int m_gridWidth = 0;
  int m_gridHeight = 0;
  std::vector<float> m_heights;
  std::unique_ptr<Mesh> m_mesh;
};

} // namespace GL
} // namespace Render
