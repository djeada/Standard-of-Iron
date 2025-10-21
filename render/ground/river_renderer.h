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

class RiverRenderer : public IRenderPass {
public:
  RiverRenderer();
  ~RiverRenderer();

  void configure(const std::vector<Game::Map::RiverSegment> &riverSegments,
                 float tileSize);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void buildMeshes();

  std::vector<Game::Map::RiverSegment> m_riverSegments;
  float m_tileSize = 1.0f;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
};

} // namespace GL
} // namespace Render
