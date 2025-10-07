#pragma once

#include <QVector3D>
#include <memory>
#include <vector>

namespace Game::Map {

enum class TerrainType { Flat, Hill, Mountain };

struct TerrainFeature {
  TerrainType type;
  float centerX;
  float centerZ;
  float radius;
  float height;

  std::vector<QVector3D> entrances;

  float rotationDeg = 0.0f;
};

class TerrainHeightMap {
public:
  TerrainHeightMap(int width, int height, float tileSize);

  void buildFromFeatures(const std::vector<TerrainFeature> &features);

  float getHeightAt(float worldX, float worldZ) const;

  float getHeightAtGrid(int gridX, int gridZ) const;

  bool isWalkable(int gridX, int gridZ) const;

  bool isHillEntrance(int gridX, int gridZ) const;

  TerrainType getTerrainType(int gridX, int gridZ) const;

  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }
  float getTileSize() const { return m_tileSize; }

  const std::vector<float> &getHeightData() const { return m_heights; }
  const std::vector<TerrainType> &getTerrainTypes() const {
    return m_terrainTypes;
  }

private:
  int m_width;
  int m_height;
  float m_tileSize;

  std::vector<float> m_heights;
  std::vector<TerrainType> m_terrainTypes;
  std::vector<bool> m_hillEntrances;
  std::vector<bool> m_hillWalkable;

  int indexAt(int x, int z) const;
  bool inBounds(int x, int z) const;

  float calculateFeatureHeight(const TerrainFeature &feature, float worldX,
                               float worldZ) const;
};

} // namespace Game::Map
