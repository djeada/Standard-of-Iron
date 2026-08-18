#include "building_line_of_sight.h"

#include <algorithm>
#include <cmath>
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

auto first_building_intersection_fraction(const QVector3D& start,
                                          const QVector3D& end,
                                          unsigned int ignore_entity_id) -> float {
  auto const& buildings = BuildingCollisionRegistry::instance().get_all_buildings();
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

auto has_clear_building_los(const QVector3D& start,
                            const QVector3D& end,
                            unsigned int ignore_entity_id) -> bool {
  return first_building_intersection_fraction(start, end, ignore_entity_id) >= 1.0F;
}

} // namespace Game::Systems
