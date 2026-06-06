#pragma once

#include "game/core/component.h"

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
};

} // namespace Engine::Core

using Engine::Core::MovementTestAccess;
