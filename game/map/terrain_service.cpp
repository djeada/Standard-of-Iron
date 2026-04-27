#include "terrain_service.h"

#include "../systems/building_collision_registry.h"
#include "map_definition.h"
#include "terrain.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace Game::Map {

namespace {

constexpr float k_road_surface_y_offset = 0.02F;

struct SurfaceBaseSample {
  float world_y{0.0F};
  SurfaceHeightKind kind{SurfaceHeightKind::Fallback};
};

auto sample_grid_clamped(const std::vector<float> &values, int width,
                         int height, int x, int z) -> float {
  if (values.empty() || width <= 0 || height <= 0) {
    return 0.0F;
  }

  x = std::clamp(x, 0, width - 1);
  z = std::clamp(z, 0, height - 1);
  return values[static_cast<size_t>(z * width + x)];
}

auto sample_surface_base_height(const TerrainHeightMap *height_map,
                                const std::vector<RoadSegment> &road_segments,
                                float world_x, float world_z,
                                float fallback_y) -> SurfaceBaseSample {
  if (height_map == nullptr) {
    return {.world_y = fallback_y, .kind = SurfaceHeightKind::Fallback};
  }

  if (height_map->isOnBridge(world_x, world_z)) {
    if (auto const bridge_height =
            height_map->getBridgeDeckHeight(world_x, world_z);
        bridge_height.has_value()) {
      return {.world_y = *bridge_height, .kind = SurfaceHeightKind::Bridge};
    }
  }

  float const terrain_height = height_map->getHeightAt(world_x, world_z);
  for (const auto &segment : road_segments) {
    const float dx = segment.end.x() - segment.start.x();
    const float dz = segment.end.z() - segment.start.z();
    const float segment_length_sq = dx * dx + dz * dz;

    if (segment_length_sq < 0.0001F) {
      const float dist_x = world_x - segment.start.x();
      const float dist_z = world_z - segment.start.z();
      const float dist_sq = dist_x * dist_x + dist_z * dist_z;
      const float half_width = segment.width * 0.5F;
      if (dist_sq <= half_width * half_width) {
        return {.world_y = terrain_height, .kind = SurfaceHeightKind::Road};
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
      return {.world_y = terrain_height, .kind = SurfaceHeightKind::Road};
    }
  }

  return {.world_y = terrain_height, .kind = SurfaceHeightKind::Terrain};
}

} // namespace

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
  rebuild_terrain_field();
}

void TerrainService::clear() {
  m_height_map.reset();
  m_terrain_field.clear();
  m_biome_settings = BiomeSettings();
  m_fire_camps.clear();
  m_road_segments.clear();
}

auto TerrainService::get_terrain_height(float world_x,
                                        float world_z) const -> float {
  return sample_surface_height(world_x, world_z).world_y;
}

auto TerrainService::sample_surface_height(float world_x, float world_z,
                                           float fallback_y) const
    -> SurfaceHeightSample {
  auto const base_sample = sample_surface_base_height(
      m_height_map.get(), m_road_segments, world_x, world_z, fallback_y);
  if (base_sample.kind == SurfaceHeightKind::Road) {
    return {.world_y = base_sample.world_y + k_road_surface_y_offset,
            .kind = base_sample.kind};
  }
  return {.world_y = base_sample.world_y, .kind = base_sample.kind};
}

auto TerrainService::resolve_surface_world_y(float world_x, float world_z,
                                             float world_y_offset,
                                             float fallback_y) const -> float {
  auto const base_sample = sample_surface_base_height(
      m_height_map.get(), m_road_segments, world_x, world_z, fallback_y);

  double resolved_world_y = static_cast<double>(base_sample.world_y) +
                            static_cast<double>(world_y_offset);
  if (base_sample.kind == SurfaceHeightKind::Road) {
    resolved_world_y += static_cast<double>(k_road_surface_y_offset);
  }

  return static_cast<float>(resolved_world_y);
}

auto TerrainService::resolve_surface_world_position(
    float world_x, float world_z, float world_y_offset,
    float fallback_y) const -> QVector3D {
  return {world_x,
          resolve_surface_world_y(world_x, world_z, world_y_offset, fallback_y),
          world_z};
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
  return m_height_map->is_walkable(grid_x, grid_z);
}

auto TerrainService::is_forbidden(int grid_x, int grid_z) const -> bool {
  if (!m_height_map) {
    return false;
  }

  if (!m_height_map->is_walkable(grid_x, grid_z)) {
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
  return registry.is_point_in_building(world_x, world_z);
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

  return is_forbidden(grid_x_int, grid_z_int);
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
  rebuild_terrain_field();
}

auto TerrainService::is_point_on_road(float world_x,
                                      float world_z) const -> bool {
  return sample_surface_base_height(m_height_map.get(), m_road_segments,
                                    world_x, world_z, 0.0F)
             .kind == SurfaceHeightKind::Road;
}

auto TerrainService::is_on_bridge(float world_x, float world_z) const -> bool {
  if (!m_height_map) {
    return false;
  }
  return m_height_map->isOnBridge(world_x, world_z);
}

auto TerrainService::get_bridge_center_position(
    float world_x, float world_z) const -> std::optional<QVector3D> {
  if (!m_height_map) {
    return std::nullopt;
  }
  return m_height_map->getBridgeCenterPosition(world_x, world_z);
}

void TerrainService::rebuild_terrain_field() {
  m_terrain_field.clear();

  if (!m_height_map) {
    return;
  }

  m_terrain_field.width = m_height_map->getWidth();
  m_terrain_field.height = m_height_map->getHeight();
  m_terrain_field.tile_size = m_height_map->getTileSize();
  m_terrain_field.heights = m_height_map->getHeightData();

  const int width = m_terrain_field.width;
  const int height = m_terrain_field.height;
  const float tile = std::max(0.001F, m_terrain_field.tile_size);
  const size_t count = static_cast<size_t>(width * height);
  m_terrain_field.slopes.assign(count, 0.0F);
  m_terrain_field.curvature.assign(count, 0.0F);

  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width; ++x) {
      const float h_c =
          sample_grid_clamped(m_terrain_field.heights, width, height, x, z);
      const float h_l =
          sample_grid_clamped(m_terrain_field.heights, width, height, x - 1, z);
      const float h_r =
          sample_grid_clamped(m_terrain_field.heights, width, height, x + 1, z);
      const float h_d =
          sample_grid_clamped(m_terrain_field.heights, width, height, x, z - 1);
      const float h_u =
          sample_grid_clamped(m_terrain_field.heights, width, height, x, z + 1);
      const float h_dl = sample_grid_clamped(m_terrain_field.heights, width,
                                             height, x - 1, z - 1);
      const float h_dr = sample_grid_clamped(m_terrain_field.heights, width,
                                             height, x + 1, z - 1);
      const float h_ul = sample_grid_clamped(m_terrain_field.heights, width,
                                             height, x - 1, z + 1);
      const float h_ur = sample_grid_clamped(m_terrain_field.heights, width,
                                             height, x + 1, z + 1);

      const float dx = (h_r - h_l) / (2.0F * tile);
      const float dz = (h_u - h_d) / (2.0F * tile);
      const float normal_y = 1.0F / std::sqrt(1.0F + dx * dx + dz * dz);
      const size_t index = static_cast<size_t>(z * width + x);
      const float dxx = h_r - 2.0F * h_c + h_l;
      const float dzz = h_u - 2.0F * h_c + h_d;
      const float dxz = 0.25F * (h_ur - h_ul - h_dr + h_dl);
      const float curvature_scale = std::max(tile * tile, 0.0001F);

      m_terrain_field.slopes[index] = 1.0F - std::clamp(normal_y, 0.0F, 1.0F);
      m_terrain_field.curvature[index] =
          std::sqrt(dxx * dxx + dzz * dzz + 2.0F * dxz * dxz) / curvature_scale;
    }
  }
}

} // namespace Game::Map
