#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "terrain_gpu.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

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

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void set_wireframe(bool enable) { m_wireframe = enable; }

private:
  void build_meshes();
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
    static constexpr float kDefaultColorR = 0.3F;
    static constexpr float kDefaultColorG = 0.5F;
    static constexpr float kDefaultColorB = 0.3F;

    static auto default_color() -> QVector3D {
      return {kDefaultColorR, kDefaultColorG, kDefaultColorB};
    }

    QVector3D color = default_color();
    float average_height = 0.0F;
    float tint = 1.0F;
    TerrainChunkParams params;
  };

  struct ChunkVisibilityCacheEntry {
    std::uint64_t visibility_version = std::numeric_limits<std::uint64_t>::max();
    bool any_visible = true;
  };

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  bool m_wireframe = false;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  std::vector<bool> m_hill_entrances;
  std::vector<ChunkMesh> m_chunks;
  std::vector<ChunkVisibilityCacheEntry> m_chunk_visibility_cache;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
};

} // namespace Render::GL
