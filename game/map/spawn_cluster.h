#pragma once

#include <limits>
#include <optional>
#include <vector>

namespace Game::Map {

inline constexpr float k_spawn_cluster_radius = 12.0F;

struct WeightedSpawnPoint {
  float x = 0.0F;
  float z = 0.0F;
  float weight = 1.0F;
};

struct SpawnClusterCenter {
  float x = 0.0F;
  float z = 0.0F;
};

[[nodiscard]] inline auto
densest_spawn_cluster(const std::vector<WeightedSpawnPoint>& points,
                      float cluster_radius = k_spawn_cluster_radius)
    -> std::optional<SpawnClusterCenter> {
  if (points.empty()) {
    return std::nullopt;
  }

  const float cluster_radius_sq = cluster_radius * cluster_radius;
  float best_weight = -1.0F;
  float best_distance_sum = std::numeric_limits<float>::infinity();
  SpawnClusterCenter best_center{};

  for (const auto& candidate : points) {
    float cluster_weight = 0.0F;
    float weighted_sum_x = 0.0F;
    float weighted_sum_z = 0.0F;
    float distance_sum = 0.0F;

    for (const auto& point : points) {
      const float dx = point.x - candidate.x;
      const float dz = point.z - candidate.z;
      const float distance_sq = (dx * dx) + (dz * dz);
      if (distance_sq > cluster_radius_sq) {
        continue;
      }

      cluster_weight += point.weight;
      weighted_sum_x += point.x * point.weight;
      weighted_sum_z += point.z * point.weight;
      distance_sum += distance_sq * point.weight;
    }

    if (cluster_weight <= 0.0F) {
      continue;
    }

    if (cluster_weight > best_weight ||
        (cluster_weight == best_weight && distance_sum < best_distance_sum)) {
      best_weight = cluster_weight;
      best_distance_sum = distance_sum;
      const float inverse_weight = 1.0F / cluster_weight;
      best_center = {weighted_sum_x * inverse_weight, weighted_sum_z * inverse_weight};
    }
  }

  if (best_weight <= 0.0F) {
    return std::nullopt;
  }
  return best_center;
}

} // namespace Game::Map
