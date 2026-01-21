#pragma once

#include "../../game/map/terrain.h"
#include "../gl/texture.h"
#include "../i_render_pass.h"
#include <QMatrix4x4>
#include <cstdint>
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

  void configure(const std::vector<Game::Map::RiverSegment> &river_segments,
                 const Game::Map::TerrainHeightMap &height_map);

  void submit(Renderer &renderer, ResourceManager *resources) override;

private:
  void build_meshes();

  std::vector<Game::Map::RiverSegment> m_river_segments;
  float m_tile_size = 1.0F;
  int m_grid_width = 0;
  int m_grid_height = 0;
  std::vector<float> m_heights;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
  std::vector<std::vector<QVector3D>> m_visibility_samples;

  std::unique_ptr<Texture> m_visibility_texture;
  std::uint64_t m_cached_visibility_version = 0;
  int m_visibility_width = 0;
  int m_visibility_height = 0;
  float m_explored_dim_factor = 0.6F;
};

} // namespace Render::GL
