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

#include "game/map/biome_settings.h"
#include "game/map/terrain.h"
#include "render/frame/draw_commands.h"
#include "render/frame/i_render_pass.h"
#include "render/frame/mist_volume.h"
#include "render/frame/world_chunk.h"
#include "render/gl/mesh.h"
#include "render/gl/terrain_noise_atlas.h"
#include "render/gl/texture.h"
#include "render/ground/building_ground.h"
#include "render/ground/ground_ecology.h"

namespace Render::GL {
class Buffer;
class Renderer;
class ResourceManager;
class Mesh;
class Texture;
class Shader;

[[nodiscard]] auto
make_terrain_chunk_params(const Game::Map::BiomeSettings& biome_settings,
                          const Game::Map::TerrainSurfaceProfile& surface_profile,
                          const Game::Map::ClimateProfile& climate_profile,
                          float tile_size,
                          std::uint32_t noise_seed,
                          Game::Map::TerrainType chunk_type,
                          float tint) -> TerrainChunkParams;

class TerrainRenderer : public IRenderPass {
public:
  TerrainRenderer();
  ~TerrainRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings);

  void set_light_direction(const QVector3D& dir);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  [[nodiscard]] auto prewarm_gpu_resources() -> bool;

  void set_wireframe(bool enable) { m_wireframe = enable; }

private:
  void build_meshes();
  [[nodiscard]] static auto
  compute_ground_fog(const std::vector<float>& heights) -> Render::GroundFogSettings;

  [[nodiscard]] auto
  make_chunk_params(const Game::Map::TerrainSurfaceProfile& surface_profile,
                    const Game::Map::ClimateProfile& climate_profile,
                    Game::Map::TerrainType chunk_type,
                    float tint) const -> TerrainChunkParams;

  auto update_height_texture(const Render::Ground::BuildingGroundSet& building_ground)
      -> TerrainSurfaceCmd::HeightResources;
  void bake_terrain_fields();
  void bake_cover_mask();
  void stamp_building_ground();
  void bake_terrain_noise_atlas();
  void bake_terrain_microdetail();

  struct ChunkMesh {
    std::unique_ptr<Mesh> mesh;
    Game::Map::TerrainType type = Game::Map::TerrainType::Flat;
    QVector3D cull_center;
    float cull_radius = 0.0F;
    TerrainChunkParams params;
  };

  struct ChunkVisibilityCacheEntry {
    bool was_submitted = false;
  };

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  bool m_wireframe = false;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  std::vector<bool> m_hill_entrances;
  Render::GroundFogSettings m_ground_fog{};
  std::unique_ptr<Texture> m_height_texture;
  bool m_height_texture_dirty = true;
  std::unique_ptr<Texture> m_terrain_field_texture;
  std::vector<float> m_terrain_field_data;
  bool m_terrain_fields_dirty = true;
  std::unique_ptr<Texture> m_cover_texture;
  std::vector<float> m_cover_data;
  std::shared_ptr<const Render::Ground::GroundEcologyField> m_ecology;
  std::vector<Render::Ground::BuildingGroundFootprint> m_building_footprints;
  std::uint64_t m_building_ground_revision = 0U;
  bool m_cover_dirty = true;
  TerrainNoiseAtlas m_noise_atlas;
  bool m_noise_atlas_dirty = true;
  std::vector<ChunkMesh> m_chunks;
  std::vector<ChunkVisibilityCacheEntry> m_chunk_visibility_cache;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
  QVector3D m_light_direction{0.65F, 0.50F, 0.40F};
};

} // namespace Render::GL
