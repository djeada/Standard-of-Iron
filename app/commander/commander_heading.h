#pragma once

#include <cmath>

namespace App::Core {

inline constexpr float k_degrees_to_radians = 0.017453292519943295F;
inline constexpr float k_radians_to_degrees = 57.29577951308232F;

[[nodiscard]] inline auto wrap_angle_degrees(float degrees) -> float {
  degrees = std::fmod(degrees, 360.0F);
  if (degrees < 0.0F) {
    degrees += 360.0F;
  }
  return degrees;
}

[[nodiscard]] inline auto signed_angle_delta(float target_degrees,
                                             float current_degrees) -> float {
  float diff = target_degrees - current_degrees;
  while (diff > 180.0F) {
    diff -= 360.0F;
  }
  while (diff < -180.0F) {
    diff += 360.0F;
  }
  return diff;
}

[[nodiscard]] inline auto rightward_look_delta(float yaw_degrees,
                                               float previous_yaw_degrees) -> float {
  return -signed_angle_delta(yaw_degrees, previous_yaw_degrees);
}

} // namespace App::Core
