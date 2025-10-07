#pragma once

#include "../../game/map/terrain.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Renderer;
class ResourceManager;
class Mesh;
class Texture;

class TerrainRenderer {
public:
  TerrainRenderer();
  ~TerrainRenderer();

  void configure(const Game::Map::TerrainHeightMap &heightMap);

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
  };

  enum class PropType { Pebble, Tuft, Stick };

  struct PropInstance {
    PropType type = PropType::Pebble;
    QVector3D position{0.0f, 0.0f, 0.0f};
    QVector3D scale{1.0f, 1.0f, 1.0f};
    QVector3D color{0.4f, 0.4f, 0.4f};
    float alpha = 1.0f;
    float rotationDeg = 0.0f;
  };

  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;
  bool m_wireframe = false;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrainTypes;
  std::vector<ChunkMesh> m_chunks;
  std::vector<PropInstance> m_props;
};

} // namespace GL
} // namespace Render
