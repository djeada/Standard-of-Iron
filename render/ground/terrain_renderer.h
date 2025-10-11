#pragma once

#include "../../game/map/terrain.h"
#include "terrain_gpu.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Buffer;
class Renderer;
class ResourceManager;
class Mesh;
class Texture;

class TerrainRenderer {
public:
  TerrainRenderer();
  ~TerrainRenderer();

  void configure(const Game::Map::TerrainHeightMap &heightMap,
                 const Game::Map::BiomeSettings &biomeSettings);

  void submit(Renderer &renderer, ResourceManager &resources);

  void setWireframe(bool enable) { m_wireframe = enable; }

private:
  void buildMeshes();
  int sectionFor(Game::Map::TerrainType type) const;

  QVector3D getTerrainColor(Game::Map::TerrainType type, float height) const;
  struct ChunkMesh {
    std::unique_ptr<Mesh> mesh;
    int minX = 0;
    int maxX = 0;
    int minZ = 0;
    int maxZ = 0;
    Game::Map::TerrainType type = Game::Map::TerrainType::Flat;
    QVector3D color{0.3f, 0.5f, 0.3f};
    float averageHeight = 0.0f;
    float tint = 1.0f;
    TerrainChunkParams params;
  };

  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;
  bool m_wireframe = false;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrainTypes;
  std::vector<ChunkMesh> m_chunks;
  Game::Map::BiomeSettings m_biomeSettings;
  std::uint32_t m_noiseSeed = 0u;
};

} // namespace GL
} // namespace Render
