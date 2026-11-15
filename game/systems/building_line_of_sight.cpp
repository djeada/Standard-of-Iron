#include "building_line_of_sight.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "../util/planar_math.h"
#include "building_collision_registry.h"

namespace Game::Systems {

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

    if (auto const span = segment_box_span(start.x() - building.center_x,
                                           start.z() - building.center_z,
                                           delta.x(),
                                           delta.z(),
                                           building.width * 0.5F,
                                           building.depth * 0.5F)) {
      best_fraction = std::min(best_fraction, span->enter);
    }
  }
  return best_fraction;
}

auto first_building_body_intersection_fraction(
    const BuildingCollisionRegistry& registry,
    const QVector3D& start,
    const QVector3D& end,
    float radius,
    unsigned int ignore_entity_id,
    bool skip_wall_links) -> float {
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
    if (skip_wall_links && building.wall_link) {
      continue;
    }

    float const half_width = (building.body_width * 0.5F) + grow;
    float const half_depth = (building.body_depth * 0.5F) + grow;

    if (std::abs(start.x() - building.body_center_x) <= half_width &&
        std::abs(start.z() - building.body_center_z) <= half_depth) {
      return 0.0F;
    }

    if (auto const span = segment_box_span(start.x() - building.body_center_x,
                                           start.z() - building.body_center_z,
                                           delta.x(),
                                           delta.z(),
                                           half_width,
                                           half_depth)) {
      best_fraction = std::min(best_fraction, span->enter);
    }
  }
  return best_fraction;
}

auto depenetrate_from_building_bodies(const BuildingCollisionRegistry& registry,
                                      const QVector3D& point,
                                      float radius,
                                      bool skip_wall_links) -> QVector3D {
  auto const& buildings = registry.get_all_buildings();
  QVector3D resolved = point;
  float const grow = std::max(0.0F, radius);

  for (int pass = 0; pass < 4; ++pass) {
    bool moved = false;
    for (const auto& building : buildings) {
      if (!building.blocks_navigation || (skip_wall_links && building.wall_link)) {
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
                                     const QVector3D& point,
                                     bool skip_wall_links) -> float {
  auto const& buildings = registry.get_all_buildings();
  float clearance = std::numeric_limits<float>::max();
  for (const auto& building : buildings) {
    if (!building.blocks_navigation || (skip_wall_links && building.wall_link)) {
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
