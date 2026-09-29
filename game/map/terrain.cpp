#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "terrain_surface.h"

namespace Game::Map {

void TerrainField::clear() {
  width = 0;
  height = 0;
  tile_size = 1.0F;
  heights.clear();
  slopes.clear();
  curvature.clear();
}

auto TerrainField::empty() const -> bool {
  return width <= 0 || height <= 0 || heights.empty();
}

auto TerrainField::sample_height_at(float gx, float gz) const -> float {
  if (empty()) {
    return 0.0F;
  }
  return sample_triangulated_height(heights.data(), width, height, gx, gz);
}

auto TerrainField::sample_slope_at(int grid_x, int grid_z) const -> float {
  if (slopes.empty() || grid_x < 0 || grid_x >= width || grid_z < 0 ||
      grid_z >= height) {
    return 0.0F;
  }

  return slopes[static_cast<size_t>(grid_z * width + grid_x)];
}

auto TerrainField::sample_curvature_at(int grid_x, int grid_z) const -> float {
  if (curvature.empty() || grid_x < 0 || grid_x >= width || grid_z < 0 ||
      grid_z >= height) {
    return 0.0F;
  }

  return curvature[static_cast<size_t>(grid_z * width + grid_x)];
}

TerrainHeightMap::TerrainHeightMap(int width, int height, float tile_size)
    : m_width(width)
    , m_height(height)
    , m_tile_size(tile_size) {
  const int count = width * height;
  m_heights.resize(count, 0.0F);
  m_terrain_types.resize(count, TerrainType::Flat);
  m_hill_entrances.resize(count, false);
  m_hill_walkable.resize(count, false);
  m_fields.resize(count, 0);
}

auto TerrainHeightMap::get_height_at(float world_x, float world_z) const -> float {
  const float base_height = get_base_height_at(world_x, world_z);
  if (isOnBridge(world_x, world_z)) {
    if (auto const bridge_height = getBridgeDeckHeight(world_x, world_z);
        bridge_height.has_value()) {
      return *bridge_height;
    }
  }
  return base_height;
}

auto TerrainHeightMap::get_base_height_at(float world_x, float world_z) const -> float {

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  float const gx = world_x / m_tile_size + grid_half_width;
  float const gz = world_z / m_tile_size + grid_half_height;

  if (!in_bounds(int(std::floor(gx)), int(std::floor(gz)))) {
    return 0.0F;
  }

  return sample_triangulated_height(m_heights.data(), m_width, m_height, gx, gz);
}

auto TerrainHeightMap::get_height_at_grid(int grid_x, int grid_z) const -> float {
  if (!in_bounds(grid_x, grid_z)) {
    return 0.0F;
  }
  return m_heights[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::is_walkable(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }

  int const idx = indexAt(grid_x, grid_z);
  if (!m_bridge_walkable.empty() && m_bridge_walkable[idx]) {
    return true;
  }

  if (!m_water_blocked.empty() && m_water_blocked[idx]) {
    return false;
  }

  TerrainType const type = m_terrain_types[idx];

  if (type == TerrainType::Mountain) {
    return false;
  }

  if (is_water_terrain(type)) {
    return false;
  }

  if (type == TerrainType::Hill) {
    return m_hill_walkable[indexAt(grid_x, grid_z)];
  }

  return true;
}

auto TerrainHeightMap::is_fields(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z) || m_fields.empty()) {
    return false;
  }
  return m_fields[static_cast<std::size_t>(indexAt(grid_x, grid_z))] != 0;
}

auto TerrainHeightMap::getTerrainType(int grid_x, int grid_z) const -> TerrainType {
  if (!in_bounds(grid_x, grid_z)) {
    return TerrainType::Flat;
  }
  return m_terrain_types[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isRiverOrNearby(int grid_x,
                                       int grid_z,
                                       int margin) const -> bool {
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }

  if (is_water_terrain(m_terrain_types[indexAt(grid_x, grid_z)])) {
    return true;
  }

  for (int dz = -margin; dz <= margin; ++dz) {
    for (int dx = -margin; dx <= margin; ++dx) {
      if (dx == 0 && dz == 0) {
        continue;
      }
      int const nx = grid_x + dx;
      int const nz = grid_z + dz;
      if (in_bounds(nx, nz) && is_water_terrain(m_terrain_types[indexAt(nx, nz)])) {
        return true;
      }
    }
  }

  return false;
}

auto TerrainHeightMap::indexAt(int x, int z) const -> int {
  return z * m_width + x;
}

auto TerrainHeightMap::in_bounds(int x, int z) const -> bool {
  return x >= 0 && x < m_width && z >= 0 && z < m_height;
}

auto TerrainHeightMap::calculateFeatureHeight(const TerrainFeature& feature,
                                              float world_x,
                                              float world_z) -> float {
  float const dx = world_x - feature.center_x;
  float const dz = world_z - feature.center_z;
  float const dist = std::sqrt(dx * dx + dz * dz);

  if (dist > feature.radius) {
    return 0.0F;
  }

  float const t = dist / feature.radius;
  float const height_factor = (std::cos(t * std::numbers::pi) + 1.0F) * 0.5F;

  return feature.height * height_factor;
}

void TerrainHeightMap::restore_from_data(const std::vector<float>& heights,
                                         const std::vector<TerrainType>& terrain_types,
                                         const std::vector<RiverSegment>& rivers,
                                         const std::vector<Bridge>& bridges,
                                         const std::vector<Lake>& lakes,
                                         const HillNavigation& hills) {

  const auto expected_size = static_cast<size_t>(m_width * m_height);

  if (heights.size() == expected_size) {
    m_heights = heights;
  }

  if (terrain_types.size() == expected_size) {
    m_terrain_types = terrain_types;
  }

  m_hill_entrances.clear();
  m_hill_entrances.resize(expected_size, false);
  m_hill_walkable.clear();
  m_hill_walkable.resize(expected_size, true);
  m_fields.assign(expected_size, 0);
  m_hill_entrance_centerlines.clear();

  for (size_t i = 0; i < m_terrain_types.size(); ++i) {
    if (m_terrain_types[i] == TerrainType::Hill) {
      m_hill_walkable[i] = false;
    }
  }

  if (hills.walkable.size() == expected_size) {
    for (size_t i = 0; i < expected_size; ++i) {
      m_hill_walkable[i] = hills.walkable[i] != 0U;
    }
  }
  if (hills.entrances.size() == expected_size) {
    for (size_t i = 0; i < expected_size; ++i) {
      m_hill_entrances[i] = hills.entrances[i] != 0U;
    }
  }
  m_hill_entrance_centerlines = hills.entrance_centerlines;

  m_river_segments = rivers;
  m_lakes = lakes;
  m_bridges = bridges;
  for (Bridge& bridge : m_bridges) {
    bridge.width = std::max(bridge.width, k_min_bridge_width);
  }

  precompute_water_blocked();
  precompute_bridge_data();
}

} // namespace Game::Map
