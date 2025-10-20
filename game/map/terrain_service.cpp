#include "terrain_service.h"
#include "../systems/building_collision_registry.h"
#include "map_definition.h"
#include <QDebug>
#include <cmath>

namespace Game::Map {

TerrainService &TerrainService::instance() {
  static TerrainService s_instance;
  return s_instance;
}

void TerrainService::initialize(const MapDefinition &mapDef) {
  m_heightMap = std::make_unique<TerrainHeightMap>(
      mapDef.grid.width, mapDef.grid.height, mapDef.grid.tileSize);

  m_heightMap->buildFromFeatures(mapDef.terrain);
  m_heightMap->addRiverSegments(mapDef.rivers);
  m_heightMap->addBridges(mapDef.bridges);
  m_biomeSettings = mapDef.biome;
  m_heightMap->applyBiomeVariation(m_biomeSettings);
}

void TerrainService::clear() {
  m_heightMap.reset();
  m_biomeSettings = BiomeSettings();
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

bool TerrainService::isForbidden(int gridX, int gridZ) const {
  if (!m_heightMap)
    return false;

  if (!m_heightMap->isWalkable(gridX, gridZ))
    return true;

  float halfW = m_heightMap->getWidth() * 0.5f - 0.5f;
  float halfH = m_heightMap->getHeight() * 0.5f - 0.5f;
  const float tile = m_heightMap->getTileSize();
  float worldX = (static_cast<float>(gridX) - halfW) * tile;
  float worldZ = (static_cast<float>(gridZ) - halfH) * tile;

  auto &registry = Game::Systems::BuildingCollisionRegistry::instance();
  if (registry.isPointInBuilding(worldX, worldZ))
    return true;

  return false;
}

bool TerrainService::isForbiddenWorld(float worldX, float worldZ) const {
  if (!m_heightMap)
    return false;

  const float gridHalfWidth = m_heightMap->getWidth() * 0.5f - 0.5f;
  const float gridHalfHeight = m_heightMap->getHeight() * 0.5f - 0.5f;

  float gx = worldX / m_heightMap->getTileSize() + gridHalfWidth;
  float gz = worldZ / m_heightMap->getTileSize() + gridHalfHeight;

  int ix = static_cast<int>(std::round(gx));
  int iz = static_cast<int>(std::round(gz));
  return isForbidden(ix, iz);
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
