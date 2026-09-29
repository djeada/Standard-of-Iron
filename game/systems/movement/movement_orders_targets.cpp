#include "movement_orders_targets.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "body_profile.h"
#include "map/terrain_service.h"
#include "systems/navigation/walkability.h"

namespace Game::Systems::MovementTargeting {

namespace {

constexpr int k_recovery_search_radius = 16;

auto nearest_standable_world(const QVector3D& position,
                             Pathfinding::Passability passability,
                             float search_radius) -> std::optional<QVector3D> {
  BodyProfile profile;
  profile.passability = passability;
  return Walkability::nearest_standable(position, profile, search_radius);
}

} // namespace

auto passability_for(const Engine::Core::MovementComponent& movement)
    -> Pathfinding::Passability {
  return movement.get_can_enter_forest() ? Pathfinding::Passability::Light
                                         : Pathfinding::Passability::Heavy;
}

auto is_direct_path_walkable(const QVector3D& from,
                             const QVector3D& to,
                             Pathfinding::Passability passability,
                             float clearance_radius) -> bool {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->update_navigation_grid();
    return pathfinder->is_world_segment_walkable(
        from, to, passability, Pathfinding::routing_clearance(clearance_radius));
  }

  return NavGrid::is_world_position_walkable(to);
}

auto find_recovery_cell(const Pathfinding& pathfinder,
                        const Point& origin,
                        Pathfinding::Passability passability,
                        Point& recovery_cell) -> bool {
  auto const spot =
      nearest_standable_world(pathfinder.grid_to_world(origin),
                              passability,
                              static_cast<float>(k_recovery_search_radius));
  if (!spot.has_value()) {
    return false;
  }
  recovery_cell = NavGrid::world_to_grid(spot->x(), spot->z());
  return true;
}

auto resolve_walkable_direct_target(const QVector3D& target,
                                    Pathfinding::Passability passability) -> QVector3D {
  constexpr float k_target_search_radius = 64.0F;
  return nearest_standable_world(target, passability, k_target_search_radius)
      .value_or(target);
}

auto resolve_walkable_target_toward(const QVector3D& target,
                                    const QVector3D& from,
                                    Pathfinding::Passability passability) -> QVector3D {
  BodyProfile profile;
  profile.passability = passability;
  if (Walkability::can_stand(target, profile)) {
    return target;
  }
  QVector3D const nearest = resolve_walkable_direct_target(target, passability);
  QVector3D toward = from - target;
  toward.setY(0.0F);
  float const span = toward.length();
  if (span <= 1.0e-3F) {
    return nearest;
  }
  toward /= span;
  constexpr float k_step = 0.25F;
  constexpr float k_side_slack = 1.5F;
  float const nearest_gap = (nearest - target).length();
  for (float travelled = k_step; travelled <= span; travelled += k_step) {
    QVector3D const probe = target + toward * travelled;
    if (!Walkability::can_stand(probe, profile)) {
      continue;
    }
    return travelled <= nearest_gap + k_side_slack ? probe : nearest;
  }
  return nearest;
}

auto segment_traverses_navigation_portal(const QVector3D& from,
                                         const QVector3D& to) -> bool {
  auto& terrain = Game::Map::TerrainService::instance();
  auto const* height_map = terrain.get_height_map();
  if (height_map == nullptr) {
    return false;
  }

  QVector3D const delta = to - from;
  float const length = std::hypot(delta.x(), delta.z());
  float const sample_step = std::max(height_map->get_tile_size() * 0.5F, 0.25F);
  int const sample_count =
      std::max(1, static_cast<int>(std::ceil(length / sample_step)));
  for (int sample = 0; sample <= sample_count; ++sample) {
    float const t = static_cast<float>(sample) / static_cast<float>(sample_count);
    QVector3D const point = from + delta * t;
    Point const cell = NavGrid::world_to_grid(point.x(), point.z());
    if (terrain.is_on_bridge(point.x(), point.z()) ||
        terrain.is_hill_entrance(cell.x, cell.y)) {
      return true;
    }
  }
  return false;
}

auto align_portal_waypoint(const QVector3D& waypoint,
                           bool final_waypoint,
                           const std::optional<QVector3D>& previous) -> QVector3D {
  if (final_waypoint) {
    return waypoint;
  }

  const auto* pathfinder = NavGrid::get_pathfinder();
  auto usable = [pathfinder, &previous](const QVector3D& candidate) {
    if (pathfinder == nullptr) {
      return true;
    }
    Point const cell = NavGrid::world_to_grid(candidate.x(), candidate.z());
    if (!pathfinder->is_walkable(cell.x, cell.y)) {
      return false;
    }

    return !previous.has_value() ||
           pathfinder->is_world_segment_walkable(
               *previous, candidate, Pathfinding::Passability::Light, 0.0F);
  };

  auto& terrain = Game::Map::TerrainService::instance();
  if (auto const aligned =
          terrain.get_bridge_traversal_position(waypoint.x(), waypoint.z())) {
    QVector3D const candidate(aligned->x(), waypoint.y(), aligned->z());
    if (usable(candidate)) {
      return candidate;
    }
  }
  if (auto const aligned =
          terrain.get_hill_entrance_traversal_position(waypoint.x(), waypoint.z())) {
    QVector3D const candidate(aligned->x(), waypoint.y(), aligned->z());
    if (usable(candidate)) {
      return candidate;
    }
  }
  return waypoint;
}

[[nodiscard]] auto
path_legs_are_walkable(Pathfinding& pathfinder,
                       const Engine::Core::TransformComponent& transform,
                       Pathfinding::Passability passability,
                       const std::vector<std::pair<float, float>>& path) -> bool {
  QVector3D previous(transform.position.x, 0.0F, transform.position.z);
  for (auto const& waypoint : path) {
    QVector3D const point(waypoint.first, 0.0F, waypoint.second);
    if (!pathfinder.is_world_segment_walkable(previous, point, passability, 0.0F)) {
      return false;
    }
    previous = point;
  }
  return true;
}

void pull_path_taut(Pathfinding& pathfinder,
                    const Engine::Core::TransformComponent& transform,
                    Pathfinding::Passability passability,
                    float clearance_radius,
                    std::vector<std::pair<float, float>>& path) {
  if (path.size() < 3U) {
    return;
  }
  auto shortcut_allowed = [&](const QVector3D& from, const QVector3D& to) -> bool {
    return pathfinder.is_world_segment_walkable(
               from, to, passability, clearance_radius) &&
           !segment_traverses_navigation_portal(from, to);
  };

  std::vector<std::pair<float, float>> taut;
  taut.reserve(path.size());
  QVector3D anchor(transform.position.x, 0.0F, transform.position.z);
  std::size_t index = 0;
  while (index < path.size()) {
    std::size_t reach = index;
    while (reach + 1U < path.size()) {
      QVector3D const candidate(path[reach + 1U].first, 0.0F, path[reach + 1U].second);
      if (!shortcut_allowed(anchor, candidate)) {
        break;
      }
      ++reach;
    }
    taut.push_back(path[reach]);
    anchor = QVector3D(path[reach].first, 0.0F, path[reach].second);
    index = reach + 1U;
  }
  path = std::move(taut);
}

} // namespace Game::Systems::MovementTargeting
