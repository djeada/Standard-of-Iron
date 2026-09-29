#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "terrain.h"

namespace Game::Map {

auto TerrainHeightMap::isHillEntrance(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  return m_hill_entrances[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::getHillEntranceTraversalPosition(
    float world_x, float world_z) const -> std::optional<QVector3D> {
  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const int grid_x =
      static_cast<int>(std::round(world_x / m_tile_size + grid_half_width));
  const int grid_z =
      static_cast<int>(std::round(world_z / m_tile_size + grid_half_height));
  if (!isHillEntrance(grid_x, grid_z)) {
    return std::nullopt;
  }

  QVector3D const query(world_x, 0.0F, world_z);
  std::optional<QVector3D> closest;
  float closest_distance_sq = std::numeric_limits<float>::infinity();
  for (const auto& centerline : m_hill_entrance_centerlines) {
    QVector3D const delta = centerline.end - centerline.start;
    float const length_sq = QVector3D::dotProduct(delta, delta);
    if (length_sq <= 1.0e-6F) {
      continue;
    }
    float const progress = std::clamp(
        QVector3D::dotProduct(query - centerline.start, delta) / length_sq, 0.0F, 1.0F);
    QVector3D const projected = centerline.start + delta * progress;
    float const distance_sq = (projected - query).lengthSquared();
    if (distance_sq < closest_distance_sq) {
      closest_distance_sq = distance_sq;
      closest = projected;
    }
  }
  return closest;
}

auto TerrainHeightMap::hill_navigation() const -> HillNavigation {
  HillNavigation navigation;
  navigation.walkable.assign(m_hill_walkable.begin(), m_hill_walkable.end());
  navigation.entrances.assign(m_hill_entrances.begin(), m_hill_entrances.end());
  navigation.entrance_centerlines = m_hill_entrance_centerlines;
  return navigation;
}

} // namespace Game::Map
