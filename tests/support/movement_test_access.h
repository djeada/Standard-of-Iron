#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "game/core/component_core.h"

namespace Engine::Core {

struct MovementTestAccess {
  static void set_has_target(MovementComponent& m, bool v) { m.has_target = v; }
  static void set_target_x(MovementComponent& m, float v) { m.target_x = v; }
  static void set_target_y(MovementComponent& m, float v) { m.target_y = v; }
  static void set_goal_x(MovementComponent& m, float v) { m.goal_x = v; }
  static void set_goal_y(MovementComponent& m, float v) { m.goal_y = v; }
  static void set_vx(MovementComponent& m, float v) { m.vx = v; }
  static void set_vz(MovementComponent& m, float v) { m.vz = v; }
  static void set_path(MovementComponent& m, std::vector<std::pair<float, float>> v) {
    m.path = std::move(v);
  }
  static void set_path_index(MovementComponent& m, std::size_t v) { m.path_index = v; }
  static void set_route_lane_state(MovementComponent& m,
                                   float minimum_scale,
                                   std::size_t opening_waypoint,
                                   std::size_t reform_waypoint) {
    m.route_lane_min_scale = minimum_scale;
    m.route_opening_waypoint_index = opening_waypoint;
    m.route_reform_waypoint_index = reform_waypoint;
  }
  static void set_stuck_time(MovementComponent& m, float v) { m.stuck_timer = v; }
};

} // namespace Engine::Core

using Engine::Core::MovementTestAccess;
