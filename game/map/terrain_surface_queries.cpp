#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "terrain.h"
#include "terrain_service.h"
#include "terrain_surface.h"

namespace Game::Map {

namespace {

constexpr float k_min_tile_size = 0.0001F;
constexpr float k_two_pi = 6.28318530717958647692F;

auto is_point_within_linear_feature(float world_x,
                                    float world_z,
                                    const QVector3D& start,
                                    const QVector3D& end,
                                    float width,
                                    float clearance) -> bool {
  const float effective_half_width =
      std::max(0.0F, width * 0.5F + std::max(clearance, 0.0F));
  const float dx = end.x() - start.x();
  const float dz = end.z() - start.z();
  const float segment_length_sq = dx * dx + dz * dz;

  if (segment_length_sq < 0.0001F) {
    const float dist_x = world_x - start.x();
    const float dist_z = world_z - start.z();
    return dist_x * dist_x + dist_z * dist_z <=
           effective_half_width * effective_half_width;
  }

  const float px = world_x - start.x();
  const float pz = world_z - start.z();
  float t = (px * dx + pz * dz) / segment_length_sq;
  t = std::clamp(t, 0.0F, 1.0F);

  const float closest_x = start.x() + t * dx;
  const float closest_z = start.z() + t * dz;
  const float dist_x = world_x - closest_x;
  const float dist_z = world_z - closest_z;
  return dist_x * dist_x + dist_z * dist_z <=
         effective_half_width * effective_half_width;
}

} // namespace

auto TerrainService::get_terrain_height(float world_x, float world_z) const -> float {
  return sample_surface_height(world_x, world_z).world_y;
}

auto TerrainService::sample_surface_base_height(
    float world_x, float world_z, float fallback_y) const -> SurfaceHeightSample {
  if (m_height_map == nullptr) {
    return {.world_y = fallback_y, .kind = SurfaceHeightKind::Fallback};
  }

  if (m_height_map->isOnBridge(world_x, world_z)) {
    if (auto const bridge_height = m_height_map->getBridgeDeckHeight(world_x, world_z);
        bridge_height.has_value()) {
      return {.world_y = *bridge_height, .kind = SurfaceHeightKind::Bridge};
    }
  }

  const float terrain_height = m_height_map->get_base_height_at(world_x, world_z);
  if (m_road_index.is_near(world_x, world_z, 0.0F) &&
      m_height_map->ford_profile_at(world_x, world_z) == nullptr) {
    const float radius = std::max(m_height_map->get_tile_size(), k_min_tile_size) *
                         k_road_surface_envelope_tiles;
    float highest = terrain_height;
    for (int tap = 0; tap < k_road_surface_envelope_taps; ++tap) {
      const float angle = k_two_pi * static_cast<float>(tap) /
                          static_cast<float>(k_road_surface_envelope_taps);
      highest = std::max(
          highest,
          m_height_map->get_base_height_at(world_x + std::cos(angle) * radius,
                                           world_z + std::sin(angle) * radius));
    }
    return {.world_y = highest, .kind = SurfaceHeightKind::Road};
  }

  return {.world_y = terrain_height, .kind = SurfaceHeightKind::Terrain};
}

auto TerrainService::sample_surface_height(
    float world_x, float world_z, float fallback_y) const -> SurfaceHeightSample {
  auto const base_sample = sample_surface_base_height(world_x, world_z, fallback_y);
  if (base_sample.kind == SurfaceHeightKind::Road) {
    return {.world_y = road_surface_world_y(base_sample.world_y),
            .kind = base_sample.kind};
  }
  return {.world_y = base_sample.world_y, .kind = base_sample.kind};
}

auto TerrainService::resolve_surface_world_y(float world_x,
                                             float world_z,
                                             float world_y_offset,
                                             float fallback_y) const -> float {
  auto const base_sample = sample_surface_base_height(world_x, world_z, fallback_y);

  float const surface_world_y = base_sample.kind == SurfaceHeightKind::Road
                                    ? road_surface_world_y(base_sample.world_y)
                                    : base_sample.world_y;
  double const resolved_world_y =
      static_cast<double>(surface_world_y) + static_cast<double>(world_y_offset);

  return static_cast<float>(resolved_world_y);
}

auto TerrainService::resolve_surface_world_position(float world_x,
                                                    float world_z,
                                                    float world_y_offset,
                                                    float fallback_y) const
    -> QVector3D {
  return {world_x,
          resolve_surface_world_y(world_x, world_z, world_y_offset, fallback_y),
          world_z};
}

auto TerrainService::resolve_footprint_world_y(float world_x,
                                               float world_z,
                                               float footprint_radius,
                                               float world_y_offset,
                                               float fallback_y) const -> float {
  float lowest = resolve_surface_world_y(world_x, world_z, 0.0F, fallback_y);
  if (footprint_radius <= 0.0F) {
    return lowest + world_y_offset;
  }

  constexpr int k_rings = 2;
  constexpr int k_spokes = 8;
  for (int ring = 1; ring <= k_rings; ++ring) {
    const float reach =
        footprint_radius * (static_cast<float>(ring) / static_cast<float>(k_rings));
    for (int spoke = 0; spoke < k_spokes; ++spoke) {
      const float angle =
          k_two_pi * (static_cast<float>(spoke) / static_cast<float>(k_spokes));
      const float sample = resolve_surface_world_y(world_x + (std::cos(angle) * reach),
                                                   world_z + (std::sin(angle) * reach),
                                                   0.0F,
                                                   fallback_y);
      lowest = std::min(lowest, sample);
    }
  }
  return lowest + world_y_offset;
}

auto TerrainService::resolve_footprint_world_position(float world_x,
                                                      float world_z,
                                                      float footprint_radius,
                                                      float world_y_offset,
                                                      float fallback_y) const
    -> QVector3D {
  return {world_x,
          resolve_footprint_world_y(
              world_x, world_z, footprint_radius, world_y_offset, fallback_y),
          world_z};
}

auto TerrainService::sample_ground_normal(float world_x,
                                          float world_z) const -> QVector3D {
  if (m_height_map == nullptr) {
    return {0.0F, 1.0F, 0.0F};
  }
  const float tile_size = std::max(m_height_map->get_tile_size(), k_min_tile_size);
  const int width = m_height_map->get_width();
  const int height = m_height_map->get_height();
  const float gx = world_x / tile_size + (static_cast<float>(width) * 0.5F - 0.5F);
  const float gz = world_z / tile_size + (static_cast<float>(height) * 0.5F - 0.5F);
  return sample_smoothed_ground_normal(
      m_height_map->get_height_data().data(), width, height, tile_size, gx, gz);
}

auto TerrainService::get_terrain_height_grid(int grid_x, int grid_z) const -> float {
  if (!m_height_map) {
    return 0.0F;
  }
  return m_height_map->get_height_at_grid(grid_x, grid_z);
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
      static_cast<float>(m_height_map->get_width()) * k_half_cell_offset -
      k_half_cell_offset;
  const float half_height =
      static_cast<float>(m_height_map->get_height()) * k_half_cell_offset -
      k_half_cell_offset;
  const float tile_size = m_height_map->get_tile_size();

  const float world_x = (static_cast<float>(grid_x) - half_width) * tile_size;
  const float world_z = (static_cast<float>(grid_z) - half_height) * tile_size;

  return is_point_in_registered_building(world_x, world_z);
}

auto TerrainService::is_forbidden_world(float world_x, float world_z) const -> bool {
  if (!m_height_map) {
    return false;
  }

  constexpr float k_half_cell_offset = 0.5F;

  const float grid_half_width =
      static_cast<float>(m_height_map->get_width()) * k_half_cell_offset -
      k_half_cell_offset;
  const float grid_half_height =
      static_cast<float>(m_height_map->get_height()) * k_half_cell_offset -
      k_half_cell_offset;

  const float grid_x = world_x / m_height_map->get_tile_size() + grid_half_width;
  const float grid_z = world_z / m_height_map->get_tile_size() + grid_half_height;

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

auto TerrainService::get_hill_entrance_traversal_position(
    float world_x, float world_z) const -> std::optional<QVector3D> {
  if (!m_height_map) {
    return std::nullopt;
  }
  return m_height_map->getHillEntranceTraversalPosition(world_x, world_z);
}

auto TerrainService::get_terrain_type(int grid_x, int grid_z) const -> TerrainType {
  if (!m_height_map) {
    return TerrainType::Flat;
  }
  return m_height_map->getTerrainType(grid_x, grid_z);
}

auto TerrainService::is_point_on_road(float world_x, float world_z) const -> bool {
  return sample_surface_base_height(world_x, world_z, 0.0F).kind ==
         SurfaceHeightKind::Road;
}

auto TerrainService::is_point_near_road(float world_x,
                                        float world_z,
                                        float clearance) const -> bool {
  return m_road_index.is_near(world_x, world_z, clearance);
}

auto TerrainService::is_on_bridge(float world_x, float world_z) const -> bool {
  if (!m_height_map) {
    return false;
  }
  return m_height_map->isOnBridge(world_x, world_z);
}

auto TerrainService::is_point_near_bridge(float world_x,
                                          float world_z,
                                          float clearance) const -> bool {
  if (!m_height_map) {
    return false;
  }
  for (const auto& bridge : m_height_map->get_bridges()) {
    if (is_point_within_linear_feature(
            world_x, world_z, bridge.start, bridge.end, bridge.width, clearance)) {
      return true;
    }
  }
  return false;
}

auto TerrainService::is_point_near_river(float world_x,
                                         float world_z,
                                         float clearance) const -> bool {
  if (!m_height_map) {
    return false;
  }
  for (const auto& river : m_height_map->get_river_segments()) {
    if (is_point_within_linear_feature(
            world_x, world_z, river.start, river.end, river.width, clearance)) {
      return true;
    }
  }
  return false;
}

auto TerrainService::is_point_near_water(float world_x,
                                         float world_z,
                                         float clearance) const -> bool {
  if (is_point_near_river(world_x, world_z, clearance)) {
    return true;
  }
  if (!m_height_map) {
    return false;
  }
  for (const auto& lake : m_height_map->get_lakes()) {
    if (point_in_lake(lake, world_x, world_z, clearance)) {
      return true;
    }
  }
  return false;
}

auto TerrainService::get_bridge_center_position(float world_x, float world_z) const
    -> std::optional<QVector3D> {
  if (!m_height_map) {
    return std::nullopt;
  }
  return m_height_map->getBridgeCenterPosition(world_x, world_z);
}

auto TerrainService::get_bridge_traversal_position(float world_x, float world_z) const
    -> std::optional<QVector3D> {
  if (!m_height_map) {
    return std::nullopt;
  }
  return m_height_map->getBridgeTraversalPosition(world_x, world_z);
}

} // namespace Game::Map
