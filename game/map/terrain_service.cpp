#include "terrain_service.h"

#include "../systems/building_collision_registry.h"
#include "map_definition.h"
#include "terrain.h"

#include <cmath>
#include <memory>
#include <vector>

namespace Game::Map {

auto TerrainService::instance() -> TerrainService & {
  static TerrainService s_instance;
  return s_instance;
}

void TerrainService::initialize(const MapDefinition &mapDef) {
  m_height_map = std::make_unique<TerrainHeightMap>(
      mapDef.grid.width, mapDef.grid.height, mapDef.grid.tile_size);

  m_height_map->buildFromFeatures(mapDef.terrain);
  m_height_map->addRiverSegments(mapDef.rivers);
  m_height_map->addBridges(mapDef.bridges);
  m_biomeSettings = mapDef.biome;
  m_height_map->applyBiomeVariation(m_biomeSettings);
  m_fire_camps = mapDef.firecamps;
}

void TerrainService::clear() {
  m_height_map.reset();
  m_biomeSettings = BiomeSettings();
  m_fire_camps.clear();
}

auto TerrainService::getTerrainHeight(float world_x,
                                      float world_z) const -> float {
  if (!m_height_map) {
    return 0.0F;
  }
  return m_height_map->getHeightAt(world_x, world_z);
}

auto TerrainService::getTerrainHeightGrid(int grid_x,
                                          int grid_z) const -> float {
  if (!m_height_map) {
    return 0.0F;
  }
  return m_height_map->getHeightAtGrid(grid_x, grid_z);
}

auto TerrainService::isWalkable(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return true;
  }
  return m_height_map->isWalkable(grid_x, grid_z);
}

auto TerrainService::isForbidden(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return false;
  }

  if (!m_height_map->isWalkable(grid_x, grid_z)) {
    return true;
  }

  constexpr float k_half_cell_offset = 0.5F;

  const float half_width =
      static_cast<float>(m_height_map->getWidth()) * k_half_cell_offset -
      k_half_cell_offset;
  const float half_height =
      static_cast<float>(m_height_map->getHeight()) * k_half_cell_offset -
      k_half_cell_offset;
  const float tile_size = m_height_map->getTileSize();

  const float world_x = (static_cast<float>(grid_x) - half_width) * tile_size;
  const float world_z = (static_cast<float>(grid_z) - half_height) * tile_size;

  auto &registry = Game::Systems::BuildingCollisionRegistry::instance();
  return registry.isPointInBuilding(world_x, world_z);
}

auto TerrainService::isForbiddenWorld(float world_x,
                                      float world_z) const -> bool {
  if (!m_height_map) {
    return false;
  }

  constexpr float k_half_cell_offset = 0.5F;

  const float grid_half_width =
      static_cast<float>(m_height_map->getWidth()) * k_half_cell_offset -
      k_half_cell_offset;
  const float grid_half_height =
      static_cast<float>(m_height_map->getHeight()) * k_half_cell_offset -
      k_half_cell_offset;

  const float grid_x = world_x / m_height_map->getTileSize() + grid_half_width;
  const float grid_z = world_z / m_height_map->getTileSize() + grid_half_height;

  const int grid_x_int = static_cast<int>(std::round(grid_x));
  const int grid_z_int = static_cast<int>(std::round(grid_z));

  return isForbidden(grid_x_int, grid_z_int);
}

auto TerrainService::isHillEntrance(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return false;
  }
  return m_height_map->isHillEntrance(grid_x, grid_z);
}

auto TerrainService::getTerrainType(int grid_x,
                                    int grid_z) const -> TerrainType {
  if (!m_height_map) {
    return TerrainType::Flat;
  }
  return m_height_map->getTerrainType(grid_x, grid_z);
}

void TerrainService::restoreFromSerialized(
    int width, int height, float tile_size, const std::vector<float> &heights,
    const std::vector<TerrainType> &terrain_types,
    const std::vector<RiverSegment> &rivers, const std::vector<Bridge> &bridges,
    const BiomeSettings &biome) {
  m_height_map = std::make_unique<TerrainHeightMap>(width, height, tile_size);
  m_height_map->restoreFromData(heights, terrain_types, rivers, bridges);
  m_biomeSettings = biome;
}

} // namespace Game::Map
