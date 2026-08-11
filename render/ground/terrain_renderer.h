#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "game/map/terrain.h"
#include "render/draw_commands.h"
#include "render/gl/mesh.h"
#include "render/gl/texture.h"
#include "render/i_render_pass.h"
#include "render/world_chunk.h"

namespace Render::GL {
class Buffer;
class Renderer;
class ResourceManager;
class Mesh;
class Texture;

class TerrainRenderer : public IRenderPass {
public:
  TerrainRenderer();
  ~TerrainRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings);

  void set_light_direction(const QVector3D& dir);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void set_wireframe(bool enable) { m_wireframe = enable; }

private:
  void build_meshes();

  [[nodiscard]] auto compute_hill_entry_weights(
      const std::vector<float>& height_data) const -> std::vector<float>;

  [[nodiscard]] auto compute_feature_foot_weights(
      const std::vector<float>& height_data) const -> std::vector<float>;

  void smooth_terrain_normals(const std::vector<float>& height_data,
                              const std::vector<QVector3D>& face_normals,
                              std::vector<QVector3D>& normals,
                              float min_height,
                              float height_range) const;
  [[nodiscard]] auto
  make_chunk_params(const Game::Map::TerrainSurfaceProfile& surface_profile,
                    const Game::Map::ClimateProfile& climate_profile,
                    Game::Map::TerrainType chunk_type,
                    float tint) const -> TerrainChunkParams;

  struct SectionData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::unordered_map<int, unsigned int> remap;
    float height_sum = 0.0F;
    int height_count = 0;
    QVector3D normal_sum = QVector3D(0, 0, 0);
    float slope_sum = 0.0F;
    float height_var_sum = 0.0F;
    int stat_count = 0;

    float ao_sum = 0.0F;
    int ao_count = 0;
    float curvature_sum = 0.0F;
    int curvature_count = 0;
    float entry_sum = 0.0F;
    float entry_peak = 0.0F;
    int entry_count = 0;
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
    const Game::Map::TerrainSurfaceProfile& surface_profile;
    const Game::Map::ClimateProfile& climate_profile;
    const std::function<float(float, float)>& sample_height_at;
    const std::function<float(float, float)>& sample_entry_at;
    const std::function<float(float, float)>& sample_feature_foot_at;
    const std::function<float(int, int)>& sample_curvature_magnitude_at;
    const std::function<QVector3D(float, float)>& normal_from_heights_at;
    const std::function<int(Game::Map::TerrainType,
                            Game::Map::TerrainType,
                            Game::Map::TerrainType,
                            Game::Map::TerrainType)>& quad_section;
    float half_width;
    float half_height;
    float min_h;
    float height_range;
  };

  void emit_terrain_chunk(const TerrainMeshBuild& build,
                          int chunk_x,
                          int chunk_z,
                          int chunk_max_x,
                          int chunk_max_z,
                          std::size_t& total_triangles);

  void finish_chunk_section(const TerrainMeshBuild& build,
                            const SectionData& section,
                            int section_index,
                            int chunk_x,
                            int chunk_z,
                            int chunk_max_x,
                            int chunk_max_z,
                            std::size_t& total_triangles);

  auto update_height_texture() -> TerrainSurfaceCmd::HeightResources;
  [[nodiscard]] static auto section_for(Game::Map::TerrainType type) -> int;

  [[nodiscard]] auto get_terrain_color(Game::Map::TerrainType type,
                                       float height) const -> QVector3D;
  struct ChunkMesh {
    std::unique_ptr<Mesh> mesh;
    int min_x = 0;
    int max_x = 0;
    int min_z = 0;
    int max_z = 0;
    Game::Map::TerrainType type = Game::Map::TerrainType::Flat;
    static constexpr float k_default_color_r = 0.3F;
    static constexpr float k_default_color_g = 0.5F;
    static constexpr float k_default_color_b = 0.3F;

    static auto default_color() -> QVector3D {
      return {k_default_color_r, k_default_color_g, k_default_color_b};
    }

    QVector3D color = default_color();
    float average_height = 0.0F;
    float tint = 1.0F;
    QVector3D cull_center;
    float cull_radius = 0.0F;
    TerrainChunkParams params;
  };

  struct ChunkVisibilityCacheEntry {
    std::uint64_t visibility_version = std::numeric_limits<std::uint64_t>::max();
    bool any_revealed = true;
    bool was_submitted = false;
  };

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  bool m_wireframe = false;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  std::vector<bool> m_hill_entrances;
  std::unique_ptr<Texture> m_height_texture;
  bool m_height_texture_dirty = true;
  std::vector<ChunkMesh> m_chunks;
  std::vector<ChunkVisibilityCacheEntry> m_chunk_visibility_cache;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
  QVector3D m_light_direction{0.65F, 0.50F, 0.40F};
};

} // namespace Render::GL
