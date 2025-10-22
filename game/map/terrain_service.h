#pragma once

#include "terrain.h"
#include <memory>

namespace Game::Map {

struct MapDefinition;

class TerrainService {
public:
  static TerrainService &instance();

  void initialize(const MapDefinition &mapDef);

  void clear();

  float getTerrainHeight(float worldX, float worldZ) const;

  float getTerrainHeightGrid(int gridX, int gridZ) const;

  bool isWalkable(int gridX, int gridZ) const;

  bool isHillEntrance(int gridX, int gridZ) const;

  bool isForbidden(int gridX, int gridZ) const;

  bool isForbiddenWorld(float worldX, float worldZ) const;

  TerrainType getTerrainType(int gridX, int gridZ) const;

  const TerrainHeightMap *getHeightMap() const { return m_heightMap.get(); }

  const BiomeSettings &biomeSettings() const { return m_biomeSettings; }

  bool isInitialized() const { return m_heightMap != nullptr; }

  void restoreFromSerialized(int width, int height, float tileSize,
                             const std::vector<float> &heights,
                             const std::vector<TerrainType> &terrainTypes,
                             const std::vector<RiverSegment> &rivers,
                             const std::vector<Bridge> &bridges,
                             const BiomeSettings &biome);

private:
  TerrainService() = default;
  ~TerrainService() = default;

  TerrainService(const TerrainService &) = delete;
  TerrainService &operator=(const TerrainService &) = delete;

  std::unique_ptr<TerrainHeightMap> m_heightMap;
  BiomeSettings m_biomeSettings;
};

} // namespace Game::Map
