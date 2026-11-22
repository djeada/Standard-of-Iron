#pragma once

#include <QVector3D>

#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_map>
#include <vector>

#include "game/map/terrain.h"
#include "render/geom/mesh.h"

namespace Render::Ground {

struct TerrainChunkGeometry {
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  Game::Map::TerrainType type = Game::Map::TerrainType::Flat;
  QVector3D bounds_min;
  QVector3D bounds_max;
};

struct TerrainMeshInput {
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  const std::vector<float>& heights;
  const std::vector<Game::Map::TerrainType>& terrain_types;
  const std::vector<bool>& hill_entrances;
};

class TerrainMeshBuilder {
public:
  explicit TerrainMeshBuilder(const TerrainMeshInput& input);

  [[nodiscard]] auto build() const -> std::vector<TerrainChunkGeometry>;

private:
  struct SectionData {
    std::vector<Render::GL::Vertex> vertices;
    std::vector<unsigned int> indices;
    std::unordered_map<int, unsigned int> remap;
    QVector3D bounds_min{std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max(),
                         std::numeric_limits<float>::max()};
    QVector3D bounds_max{std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest(),
                         std::numeric_limits<float>::lowest()};
  };

  struct TerrainMeshBuild {
    const std::vector<QVector3D>& positions;
    const std::vector<QVector3D>& normals;
    const std::vector<float>& height_data;
    const std::vector<float>& feature_foot_weight;
    const std::vector<float>& entry_weight;
    const std::vector<float>& hill_weight;
    const std::vector<float>& mountain_weight;
    const std::function<float(float, float)>& sample_height_at;
    const std::function<QVector3D(float, float)>& normal_from_heights_at;
    const std::function<int(Game::Map::TerrainType,
                            Game::Map::TerrainType,
                            Game::Map::TerrainType,
                            Game::Map::TerrainType)>& quad_section;
    float half_width;
    float half_height;
  };

  [[nodiscard]] static auto section_for(Game::Map::TerrainType type) -> int;

  [[nodiscard]] auto compute_hill_entry_weights(
      const std::vector<float>& height_data) const -> std::vector<float>;

  [[nodiscard]] auto compute_feature_foot_weights(
      const std::vector<float>& height_data) const -> std::vector<float>;

  void smooth_terrain_normals(const std::vector<float>& height_data,
                              const std::vector<QVector3D>& face_normals,
                              std::vector<QVector3D>& normals,
                              float min_height,
                              float height_range) const;

  void emit_terrain_chunk(const TerrainMeshBuild& build,
                          int chunk_x,
                          int chunk_z,
                          int chunk_max_x,
                          int chunk_max_z,
                          std::vector<TerrainChunkGeometry>& out) const;

  int m_width;
  int m_height;
  float m_tile_size;
  const std::vector<float>& m_height_data;
  const std::vector<Game::Map::TerrainType>& m_terrain_types;
  const std::vector<bool>& m_hill_entrances;
};

} // namespace Render::Ground
