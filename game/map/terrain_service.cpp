#include "terrain_service.h"

#include "../systems/building_collision_registry.h"
#include "map_definition.h"
#include "terrain.h"

#include <algorithm>
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
  m_biome_settings = mapDef.biome;
  m_height_map->applyBiomeVariation(m_biome_settings);
  m_fire_camps = mapDef.firecamps;
  m_road_segments = mapDef.roads;
}

void TerrainService::clear() {
  m_height_map.reset();
  m_biome_settings = BiomeSettings();
  m_fire_camps.clear();
  m_road_segments.clear();
}

auto TerrainService::get_terrain_height(float world_x,
                                      float world_z) const -> float {
  if (!m_height_map) {
    return 0.0F;
  }
  return m_height_map->getHeightAt(world_x, world_z);
}

auto TerrainService::get_terrain_height_grid(int grid_x,
                                          int grid_z) const -> float {
  if (!m_height_map) {
    return 0.0F;
  }
  return m_height_map->getHeightAtGrid(grid_x, grid_z);
}

auto TerrainService::is_walkable(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return true;
  }
  return m_height_map->isWalkable(grid_x, grid_z);
}

auto TerrainService::is_forbidden(int grid_x, int grid_z) const -> bool {
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

auto TerrainService::is_forbidden_world(float world_x,
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

auto TerrainService::is_hill_entrance(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return false;
  }
  return m_height_map->isHillEntrance(grid_x, grid_z);
}

auto TerrainService::get_terrain_type(int grid_x,
                                    int grid_z) const -> TerrainType {
  if (!m_height_map) {
    return TerrainType::Flat;
  }
  return m_height_map->getTerrainType(grid_x, grid_z);
}

void TerrainService::restore_from_serialized(
    int width, int height, float tile_size, const std::vector<float> &heights,
    const std::vector<TerrainType> &terrain_types,
    const std::vector<RiverSegment> &rivers,
    const std::vector<RoadSegment> &roads, const std::vector<Bridge> &bridges,
    const BiomeSettings &biome) {
  m_height_map = std::make_unique<TerrainHeightMap>(width, height, tile_size);
  m_height_map->restoreFromData(heights, terrain_types, rivers, bridges);
  m_biome_settings = biome;
  m_road_segments = roads;
}

auto TerrainService::is_point_on_road(float world_x,
                                      float world_z) const -> bool {
  for (const auto &segment : m_road_segments) {

    const float dx = segment.end.x() - segment.start.x();
    const float dz = segment.end.z() - segment.start.z();
    const float segment_length_sq = dx * dx + dz * dz;

    if (segment_length_sq < 0.0001F) {

      const float dist_x = world_x - segment.start.x();
      const float dist_z = world_z - segment.start.z();
      const float dist_sq = dist_x * dist_x + dist_z * dist_z;
      const float half_width = segment.width * 0.5F;
      if (dist_sq <= half_width * half_width) {
        return true;
      }
      continue;
    }

    const float px = world_x - segment.start.x();
    const float pz = world_z - segment.start.z();
    float t = (px * dx + pz * dz) / segment_length_sq;
    t = std::clamp(t, 0.0F, 1.0F);

    const float closest_x = segment.start.x() + t * dx;
    const float closest_z = segment.start.z() + t * dz;

    const float dist_x = world_x - closest_x;
    const float dist_z = world_z - closest_z;
    const float dist_sq = dist_x * dist_x + dist_z * dist_z;

    const float half_width = segment.width * 0.5F;
    if (dist_sq <= half_width * half_width) {
      return true;
    }
  }
  return false;
}

} // namespace Game::Map
