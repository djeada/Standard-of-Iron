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

class BridgeRenderer : public IRenderPass {
public:
  BridgeRenderer();
  ~BridgeRenderer();

  void configure(const std::vector<Game::Map::Bridge> &bridges, float tileSize);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void buildMeshes();

  std::vector<Game::Map::Bridge> m_bridges;
  float m_tileSize = 1.0f;
  std::unique_ptr<Mesh> m_mesh;
};

} // namespace GL
} // namespace Render
