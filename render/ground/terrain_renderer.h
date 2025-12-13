#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "terrain_gpu.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <cstdint>
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

  void setWireframe(bool enable) { m_wireframe = enable; }

private:
  void buildMeshes();
  [[nodiscard]] static auto sectionFor(Game::Map::TerrainType type) -> int;

  [[nodiscard]] auto getTerrainColor(Game::Map::TerrainType type,
                                     float height) const -> QVector3D;
  struct ChunkMesh {
    std::unique_ptr<Mesh> mesh;
    int minX = 0;
    int maxX = 0;
    int minZ = 0;
    int maxZ = 0;
    Game::Map::TerrainType type = Game::Map::TerrainType::Flat;
    static constexpr float kDefaultColorR = 0.3F;
    static constexpr float kDefaultColorG = 0.5F;
    static constexpr float kDefaultColorB = 0.3F;

    static auto defaultColor() -> QVector3D {
      return {kDefaultColorR, kDefaultColorG, kDefaultColorB};
    }

    QVector3D color = defaultColor();
    float averageHeight = 0.0F;
    float tint = 1.0F;
    TerrainChunkParams params;
  };

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  bool m_wireframe = false;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  std::vector<ChunkMesh> m_chunks;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;
};

} // namespace Render::GL
