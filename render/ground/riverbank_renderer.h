#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <memory>
#include <vector>

#include "../../game/map/terrain.h"
#include "../gl/texture.h"
#include "../i_render_pass.h"
#include "../terrain_scene_types.h"

namespace Render::GL {
class Mesh;
class Renderer;
class ResourceManager;

class ShorelineRenderer : public IRenderPass {
public:
  ShorelineRenderer();
  ~ShorelineRenderer() override;

  void configure(const std::vector<Game::Map::RiverSegment>& river_segments,
                 const std::vector<Game::Map::Lake>& lakes,
                 const Game::Map::TerrainHeightMap& height_map);

  void submit(Renderer& renderer, ResourceManager* resources) override;

private:
  void build_meshes(const Game::Map::TerrainHeightMap& height_map);

  std::vector<Game::Map::RiverSegment> m_river_segments;
  std::vector<Game::Map::Lake> m_lakes;
  float m_tile_size = 1.0F;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
  std::vector<std::vector<QVector3D>> m_visibility_samples;
  std::vector<WaterSurfaceKind> m_water_kinds;

  std::unique_ptr<Texture> m_visibility_texture;
  std::uint64_t m_cached_visibility_version = 0;
  int m_visibility_width = 0;
  int m_visibility_height = 0;
  float m_explored_dim_factor = 0.6F;
};

} // namespace Render::GL
