#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <memory>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/terrain.h"
#include "render/frame/i_draw_source.h"
#include "render/frame/terrain_scene_types.h"
#include "render/geom/mesh.h"

namespace Render::GL {
class Mesh;
class FrameSubmitter;
class DrawResources;

class ShorelineRenderer : public IDrawSource {
public:
  ShorelineRenderer();
  ~ShorelineRenderer() override;

  void configure(const std::vector<Game::Map::RiverSegment>& river_segments,
                 const std::vector<Game::Map::Lake>& lakes,
                 const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings);

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

  // Every mesh this renderer draws, for prewarming a backend's MeshGpuTable.
  void for_each_mesh(const MeshVisitor& visit) const;

private:
  void build_meshes(const Game::Map::TerrainHeightMap& height_map);

  std::vector<Game::Map::RiverSegment> m_river_segments;
  std::vector<Game::Map::Lake> m_lakes;
  float m_tile_size = 1.0F;
  Game::Map::BiomeSettings m_biome_settings;
  std::vector<std::unique_ptr<Mesh>> m_meshes;
  std::vector<std::vector<QVector3D>> m_visibility_samples;
  std::vector<WaterSurfaceKind> m_water_kinds;

  int m_visibility_width = 0;
  int m_visibility_height = 0;
  float m_explored_dim_factor = 0.82F;
};

} // namespace Render::GL
