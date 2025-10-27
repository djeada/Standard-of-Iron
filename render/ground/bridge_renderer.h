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

class BridgeRenderer : public IRenderPass {
public:
  BridgeRenderer();
  ~BridgeRenderer() override;

  void configure(const std::vector<Game::Map::Bridge> &bridges,
                 float tile_size);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void buildMeshes();

  std::vector<Game::Map::Bridge> m_bridges;
  float m_tile_size = 1.0F;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
};

} // namespace Render::GL
