#pragma once

#include <QVector3D>

#include <optional>

#include "nav_grid_types.h"

namespace Game::Systems {

class Pathfinding;

class NavGrid {
public:
  static void initialize(int world_width, int world_height);

  [[nodiscard]] static auto get_pathfinder() -> Pathfinding*;

  [[nodiscard]] static auto world_to_grid(float world_x, float world_z) -> Point;
  [[nodiscard]] static auto grid_to_world(const Point& grid_pos) -> QVector3D;
  [[nodiscard]] static auto is_grid_walkable(const Point& grid_pos) -> bool;
  [[nodiscard]] static auto
  is_world_position_walkable(const QVector3D& world_position) -> bool;
  [[nodiscard]] static auto
  find_nearest_walkable_grid(const Point& origin,
                             int max_search_radius) -> std::optional<Point>;

  [[nodiscard]] static auto
  find_nearest_walkable_grid_facing(const Point& origin,
                                    const QVector3D& approach_from,
                                    int max_search_radius) -> std::optional<Point>;
  [[nodiscard]] static auto
  snap_to_walkable_ground(const QVector3D& world_position) -> QVector3D;
  [[nodiscard]] static auto snap_to_walkable_ground(const QVector3D& world_position,
                                                    int max_search_radius) -> QVector3D;
};

} // namespace Game::Systems
