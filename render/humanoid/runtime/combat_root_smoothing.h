#pragma once

#include <algorithm>
#include <cmath>

namespace Render::Humanoid {

struct CombatRootSmoothingState {
  bool valid{false};
  float time{0.0F};
  float offset_x{0.0F};
  float offset_z{0.0F};
  float pitch_degrees{0.0F};
  float roll_degrees{0.0F};
};

struct CombatRootSmoothingTarget {
  float time{0.0F};
  float offset_x{0.0F};
  float offset_z{0.0F};
  float pitch_degrees{0.0F};
  float roll_degrees{0.0F};
};

inline constexpr float k_combat_root_translation_speed = 3.6F;
inline constexpr float k_combat_root_rotation_speed_degrees = 240.0F;
inline constexpr float k_combat_root_max_dt = 0.1F;

[[nodiscard]] inline auto smooth_combat_root(CombatRootSmoothingState& state,
                                             const CombatRootSmoothingTarget& target)
    -> CombatRootSmoothingTarget {
  if (!state.valid) {
    state.valid = true;
    state.time = target.time;
    state.offset_x = target.offset_x;
    state.offset_z = target.offset_z;
    state.pitch_degrees = target.pitch_degrees;
    state.roll_degrees = target.roll_degrees;
    return target;
  }
  float const dt = std::clamp(target.time - state.time, 0.0F, k_combat_root_max_dt);
  state.time = std::max(state.time, target.time);

  float const dx = target.offset_x - state.offset_x;
  float const dz = target.offset_z - state.offset_z;
  float const distance = std::hypot(dx, dz);
  float const max_step = k_combat_root_translation_speed * dt;
  if (distance <= max_step || distance <= 1.0e-6F) {
    state.offset_x = target.offset_x;
    state.offset_z = target.offset_z;
  } else {
    state.offset_x += dx / distance * max_step;
    state.offset_z += dz / distance * max_step;
  }

  float const max_turn = k_combat_root_rotation_speed_degrees * dt;
  state.pitch_degrees +=
      std::clamp(target.pitch_degrees - state.pitch_degrees, -max_turn, max_turn);
  state.roll_degrees +=
      std::clamp(target.roll_degrees - state.roll_degrees, -max_turn, max_turn);

  CombatRootSmoothingTarget applied = target;
  applied.offset_x = state.offset_x;
  applied.offset_z = state.offset_z;
  applied.pitch_degrees = state.pitch_degrees;
  applied.roll_degrees = state.roll_degrees;
  return applied;
}

} // namespace Render::Humanoid
