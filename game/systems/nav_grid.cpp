#include "nav_grid.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include "gate_service.h"
#include "pathfinding.h"
#include "walkability.h"

namespace Game::Systems {

std::unique_ptr<Pathfinding> NavGrid::s_pathfinder = nullptr;

void NavGrid::initialize(int world_width, int world_height) {

  GateService::clear_blockers();
  s_pathfinder = std::make_unique<Pathfinding>(world_width, world_height);

  float const offset_x = -(world_width * 0.5F - 0.5F);
  float const offset_z = -(world_height * 0.5F - 0.5F);
  s_pathfinder->set_grid_offset(offset_x, offset_z);

  BuildingCollisionRegistry::set_region_dirty_hook(
      [](float center_x, float center_z, float width, float depth) {
        if (s_pathfinder != nullptr) {
          s_pathfinder->mark_building_region_dirty(center_x, center_z, width, depth);
        }
      });
  BuildingCollisionRegistry::set_grid_dirty_hook([]() {
    if (s_pathfinder != nullptr) {
      s_pathfinder->mark_navigation_grid_dirty();
    }
  });
  BuildingCollisionRegistry::set_obstruction_released_hook([]() {
    if (s_pathfinder != nullptr) {
      s_pathfinder->mark_obstruction_released();
    }
  });
}

auto NavGrid::get_pathfinder() -> Pathfinding* {
  return s_pathfinder.get();
}
auto NavGrid::world_to_grid(float world_x, float world_z) -> Point {
  if (s_pathfinder) {
    return s_pathfinder->world_to_grid(world_x, world_z);
  }

  return {static_cast<int>(std::round(world_x)), static_cast<int>(std::round(world_z))};
}

auto NavGrid::grid_to_world(const Point& grid_pos) -> QVector3D {
  if (s_pathfinder) {
    return s_pathfinder->grid_to_world(grid_pos);
  }
  return {static_cast<float>(grid_pos.x), 0.0F, static_cast<float>(grid_pos.y)};
}

auto NavGrid::is_grid_walkable(const Point& grid_pos) -> bool {
  if (s_pathfinder != nullptr) {
    s_pathfinder->update_navigation_grid();
    return s_pathfinder->is_walkable(grid_pos.x, grid_pos.y);
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
  if (terrain_service.is_initialized()) {
    return terrain_service.is_walkable(grid_pos.x, grid_pos.y);
  }

  return true;
}

auto NavGrid::is_world_position_walkable(const QVector3D& world_position) -> bool {
  Point const grid = world_to_grid(world_position.x(), world_position.z());
  return is_grid_walkable(grid);
}

namespace {

auto cell_profile() -> BodyProfile {
  BodyProfile profile;

  profile.radius = 0.0F;
  return profile;
}

} // namespace

auto NavGrid::find_nearest_walkable_grid(const Point& origin, int max_search_radius)
    -> std::optional<Point> {
  if (max_search_radius < 0) {
    return std::nullopt;
  }
  auto const spot = Walkability::nearest_standable(
      grid_to_world(origin), cell_profile(), static_cast<float>(max_search_radius));
  if (!spot.has_value()) {
    return std::nullopt;
  }
  return world_to_grid(spot->x(), spot->z());
}

auto NavGrid::find_nearest_walkable_grid_facing(const Point& origin,
                                                const QVector3D& approach_from,
                                                int max_search_radius)
    -> std::optional<Point> {
  if (max_search_radius < 0) {
    return std::nullopt;
  }
  auto const spot =
      Walkability::nearest_standable(grid_to_world(origin),
                                     cell_profile(),
                                     static_cast<float>(max_search_radius),
                                     approach_from);
  if (!spot.has_value()) {
    return std::nullopt;
  }
  return world_to_grid(spot->x(), spot->z());
}

auto NavGrid::snap_to_walkable_ground(const QVector3D& world_position) -> QVector3D {
  return snap_to_walkable_ground(world_position, 24);
}

auto NavGrid::snap_to_walkable_ground(const QVector3D& world_position,
                                      int max_search_radius) -> QVector3D {
  QVector3D snapped = world_position;
  auto& terrain_service = Game::Map::TerrainService::instance();
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));

  Point const grid = world_to_grid(snapped.x(), snapped.z());
  auto const nearest = find_nearest_walkable_grid(grid, max_search_radius);
  if (!nearest.has_value()) {
    return snapped;
  }

  QVector3D const nearest_world = grid_to_world(*nearest);
  snapped.setX(nearest_world.x());
  snapped.setZ(nearest_world.z());
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));
  return snapped;
}

} // namespace Game::Systems
