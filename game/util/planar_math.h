#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Game::Systems {

[[nodiscard]] inline auto planar_length(float x, float z) noexcept -> float {
  return std::sqrt((x * x) + (z * z));
}

[[nodiscard]] inline auto yaw_degrees_from_direction(float dx,
                                                     float dz) noexcept -> float {
  return std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
}

[[nodiscard]] inline auto signed_yaw_delta(float from_degrees,
                                           float to_degrees) noexcept -> float {

  return std::remainder(to_degrees - from_degrees, 360.0F);
}

[[nodiscard]] inline auto turn_yaw_toward(float current_degrees,
                                          float target_degrees,
                                          float max_step_degrees) noexcept -> float {
  float const delta = signed_yaw_delta(current_degrees, target_degrees);
  float const step = std::max(0.0F, max_step_degrees);
  return current_degrees + std::clamp(delta, -step, step);
}

} // namespace Game::Systems
