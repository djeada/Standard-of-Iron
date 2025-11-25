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

class RoadRenderer : public IRenderPass {
public:
  RoadRenderer();
  ~RoadRenderer() override;

  void configure(const std::vector<Game::Map::RoadSegment> &roadSegments,
                 float tile_size);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void buildMeshes();

  std::vector<Game::Map::RoadSegment> m_roadSegments;
  float m_tile_size = 1.0F;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
};

} // namespace Render::GL
