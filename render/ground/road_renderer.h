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

  void configure(const std::vector<Game::Map::RoadSegment> &road_segments,
                 float tile_size);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void build_meshes();

  std::vector<Game::Map::RoadSegment> m_road_segments;
  float m_tile_size = 1.0F;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
};

} 
