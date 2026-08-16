#pragma once

#include <algorithm>

namespace Game {

struct CameraFraming {
  float distance = 0.0F;
  float pitch = 0.0F;
  float yaw = 0.0F;
};

inline constexpr float k_reset_distance_divisor = 3.0F;
inline constexpr float k_min_reset_distance = 24.0F;

[[nodiscard]] constexpr auto
reset_framing(const CameraFraming& authored) -> CameraFraming {
  const float eased = authored.distance / k_reset_distance_divisor;
  return {.distance =
              std::min(authored.distance, std::max(k_min_reset_distance, eased)),
          .pitch = authored.pitch,
          .yaw = authored.yaw};
}

} // namespace Game
