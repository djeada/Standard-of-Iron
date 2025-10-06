#include "terrain_service.h"
#include "map_definition.h"
#include <QDebug>

namespace Game::Map {

TerrainService &TerrainService::instance() {
  static TerrainService s_instance;
  return s_instance;
}

void TerrainService::initialize(const MapDefinition &mapDef) {
  m_heightMap = std::make_unique<TerrainHeightMap>(
      mapDef.grid.width, mapDef.grid.height, mapDef.grid.tileSize);

  m_heightMap->buildFromFeatures(mapDef.terrain);

  qDebug() << "TerrainService initialized with" << mapDef.terrain.size()
           << "terrain features";
}

float TerrainService::getTerrainHeight(float worldX, float worldZ) const {
  if (!m_heightMap)
    return 0.0f;
  return m_heightMap->getHeightAt(worldX, worldZ);
}

float TerrainService::getTerrainHeightGrid(int gridX, int gridZ) const {
  if (!m_heightMap)
    return 0.0f;
  return m_heightMap->getHeightAtGrid(gridX, gridZ);
}

bool TerrainService::isWalkable(int gridX, int gridZ) const {
  if (!m_heightMap)
    return true;
  return m_heightMap->isWalkable(gridX, gridZ);
}

bool TerrainService::isHillEntrance(int gridX, int gridZ) const {
  if (!m_heightMap)
    return false;
  return m_heightMap->isHillEntrance(gridX, gridZ);
}

TerrainType TerrainService::getTerrainType(int gridX, int gridZ) const {
  if (!m_heightMap)
    return TerrainType::Flat;
  return m_heightMap->getTerrainType(gridX, gridZ);
}

} // namespace Game::Map
