#include "building_line_of_sight.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "building_collision_registry.h"

namespace Game::Systems {

namespace {

auto slab_intersection(float start,
                       float delta,
                       float min_bound,
                       float max_bound,
                       float& t_enter,
                       float& t_exit) -> bool {
  constexpr float k_epsilon = 0.00001F;
  if (std::abs(delta) <= k_epsilon) {
    return start >= min_bound && start <= max_bound;
  }

  const float inv_delta = 1.0F / delta;
  float t0 = (min_bound - start) * inv_delta;
  float t1 = (max_bound - start) * inv_delta;
  if (t0 > t1) {
    std::swap(t0, t1);
  }
  t_enter = std::max(t_enter, t0);
  t_exit = std::min(t_exit, t1);
  return t_enter <= t_exit;
}

} // namespace

auto first_building_intersection_fraction(const BuildingCollisionRegistry& registry,
                                          const QVector3D& start,
                                          const QVector3D& end,
                                          unsigned int ignore_entity_id) -> float {
  auto const& buildings = registry.get_all_buildings();
  float best_fraction = 1.0F;
  const QVector3D delta = end - start;
  for (const auto& building : buildings) {
    if (ignore_entity_id != 0 && building.entity_id == ignore_entity_id) {
      continue;
    }

    float t_enter = 0.0F;
    float t_exit = 1.0F;
    const float half_width = building.width * 0.5F;
    const float half_depth = building.depth * 0.5F;
    if (!slab_intersection(start.x(),
                           delta.x(),
                           building.center_x - half_width,
                           building.center_x + half_width,
                           t_enter,
                           t_exit) ||
        !slab_intersection(start.z(),
                           delta.z(),
                           building.center_z - half_depth,
                           building.center_z + half_depth,
                           t_enter,
                           t_exit)) {
      continue;
    }

    if (t_enter >= 0.0F && t_enter <= 1.0F) {
      best_fraction = std::min(best_fraction, t_enter);
    }
  }
  return best_fraction;
}

auto first_building_body_intersection_fraction(
    const BuildingCollisionRegistry& registry,
    const QVector3D& start,
    const QVector3D& end,
    float radius,
    unsigned int ignore_entity_id) -> float {
  auto const& buildings = registry.get_all_buildings();
  float best_fraction = 1.0F;
  const QVector3D delta = end - start;
  float const grow = std::max(0.0F, radius);

  for (const auto& building : buildings) {
    if (!building.blocks_navigation) {
      continue;
    }
    if (ignore_entity_id != 0 && building.entity_id == ignore_entity_id) {
      continue;
    }

    float const half_width = (building.body_width * 0.5F) + grow;
    float const half_depth = (building.body_depth * 0.5F) + grow;

    if (std::abs(start.x() - building.body_center_x) <= half_width &&
        std::abs(start.z() - building.body_center_z) <= half_depth) {
      return 0.0F;
    }

    float t_enter = 0.0F;
    float t_exit = 1.0F;
    if (!slab_intersection(start.x(),
                           delta.x(),
                           building.body_center_x - half_width,
                           building.body_center_x + half_width,
                           t_enter,
                           t_exit) ||
        !slab_intersection(start.z(),
                           delta.z(),
                           building.body_center_z - half_depth,
                           building.body_center_z + half_depth,
                           t_enter,
                           t_exit)) {
      continue;
    }

    if (t_enter >= 0.0F && t_enter <= 1.0F) {
      best_fraction = std::min(best_fraction, t_enter);
    }
  }
  return best_fraction;
}

auto depenetrate_from_building_bodies(const BuildingCollisionRegistry& registry,
                                      const QVector3D& point,
                                      float radius) -> QVector3D {
  auto const& buildings = registry.get_all_buildings();
  QVector3D resolved = point;
  float const grow = std::max(0.0F, radius);

  for (int pass = 0; pass < 4; ++pass) {
    bool moved = false;
    for (const auto& building : buildings) {
      if (!building.blocks_navigation) {
        continue;
      }
      float const half_width = (building.body_width * 0.5F) + grow;
      float const half_depth = (building.body_depth * 0.5F) + grow;
      float const offset_x = resolved.x() - building.body_center_x;
      float const offset_z = resolved.z() - building.body_center_z;
      float const overlap_x = half_width - std::abs(offset_x);
      float const overlap_z = half_depth - std::abs(offset_z);
      if (overlap_x <= 0.0F || overlap_z <= 0.0F) {
        continue;
      }

      if (overlap_x <= overlap_z) {
        float const sign = offset_x >= 0.0F ? 1.0F : -1.0F;
        resolved.setX(building.body_center_x + (sign * half_width));
      } else {
        float const sign = offset_z >= 0.0F ? 1.0F : -1.0F;
        resolved.setZ(building.body_center_z + (sign * half_depth));
      }
      moved = true;
    }
    if (!moved) {
      break;
    }
  }
  return resolved;
}

auto nearest_building_body_clearance(const BuildingCollisionRegistry& registry,
                                     const QVector3D& point) -> float {
  auto const& buildings = registry.get_all_buildings();
  float clearance = std::numeric_limits<float>::max();
  for (const auto& building : buildings) {
    if (!building.blocks_navigation) {
      continue;
    }
    float const half_width = building.body_width * 0.5F;
    float const half_depth = building.body_depth * 0.5F;
    float const offset_x = std::abs(point.x() - building.body_center_x) - half_width;
    float const offset_z = std::abs(point.z() - building.body_center_z) - half_depth;

    float distance = 0.0F;
    if (offset_x > 0.0F || offset_z > 0.0F) {
      float const outside_x = std::max(offset_x, 0.0F);
      float const outside_z = std::max(offset_z, 0.0F);
      distance = std::sqrt((outside_x * outside_x) + (outside_z * outside_z));
    } else {
      distance = std::max(offset_x, offset_z);
    }
    clearance = std::min(clearance, distance);
  }
  return clearance;
}

auto has_clear_building_los(const BuildingCollisionRegistry& buildings,
                            const QVector3D& start,
                            const QVector3D& end,
                            unsigned int ignore_entity_id) -> bool {
  return first_building_body_intersection_fraction(
             buildings, start, end, 0.0F, ignore_entity_id) >= 1.0F;
}

} // namespace Game::Systems
