#pragma once

#include <QMatrix4x4>
#include <QVector4D>

#include <cstddef>
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

class WaterRenderer : public IDrawSource {
public:
  WaterRenderer();
  ~WaterRenderer() override;

  void configure(const std::vector<Game::Map::RiverSegment>& river_segments,
                 const std::vector<Game::Map::Lake>& lakes,
                 const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings);

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

  // Every mesh this renderer draws, for prewarming a backend's MeshGpuTable.
  void for_each_mesh(const MeshVisitor& visit) const;

private:
  void build_meshes();

  std::vector<Game::Map::RiverSegment> m_river_segments;
  std::vector<Game::Map::Lake> m_lakes;
  float m_tile_size = 1.0F;
  const Game::Map::TerrainHeightMap* m_height_map = nullptr;
  Game::Map::BiomeSettings m_biome_settings;
  struct FordPatch {
    QVector4D a;
    QVector4D b;
  };
  struct WaterNeighbor {
    QVector4D segment;
    float radius = 0.0F;
  };
  struct SurfaceMesh {
    std::unique_ptr<Mesh> mesh;
    WaterSurfaceKind kind = WaterSurfaceKind::River;
    QVector3D visibility_start;
    QVector3D visibility_end;
    std::vector<FordPatch> fords;
    std::vector<WaterNeighbor> neighbors;
  };
  [[nodiscard]] auto fords_near(const QVector3D& start,
                                const QVector3D& end,
                                float width) const -> std::vector<FordPatch>;
  [[nodiscard]] auto
  water_neighbors_near(const QVector3D& start,
                       const QVector3D& end,
                       float reach,
                       std::ptrdiff_t own_river,
                       std::ptrdiff_t own_lake) const -> std::vector<WaterNeighbor>;
  std::vector<SurfaceMesh> m_meshes;
};

} // namespace Render::GL
