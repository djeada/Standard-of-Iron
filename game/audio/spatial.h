#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

namespace Game::Audio {

struct WorldPoint {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

struct AudioListener {
  WorldPoint position;
  float right_x{1.0F};
  float right_z{0.0F};
  bool valid{false};
};

struct SpatialGain {
  float volume_scale{1.0F};
  float pan{0.0F};
};

inline constexpr float k_full_volume_radius = 14.0F;
inline constexpr float k_silence_radius = 90.0F;
inline constexpr float k_max_pan = 0.85F;
inline constexpr float k_pan_radius = 26.0F;

inline auto spatialize(const AudioListener& listener,
                       const WorldPoint& source) -> SpatialGain {
  if (!listener.valid) {
    return {};
  }

  const float dx = source.x - listener.position.x;
  const float dy = source.y - listener.position.y;
  const float dz = source.z - listener.position.z;
  const float distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));

  SpatialGain gain;
  if (distance <= k_full_volume_radius) {
    gain.volume_scale = 1.0F;
  } else if (distance >= k_silence_radius) {
    gain.volume_scale = 0.0F;
  } else {
    const float span = k_silence_radius - k_full_volume_radius;
    const float reach = (distance - k_full_volume_radius) / span;
    gain.volume_scale = (1.0F - reach) * (1.0F - reach);
  }

  const float lateral = (dx * listener.right_x) + (dz * listener.right_z);
  gain.pan = std::clamp(lateral / k_pan_radius, -1.0F, 1.0F) * k_max_pan;
  return gain;
}

inline auto pan_gains(float pan) -> std::pair<float, float> {
  const float clamped = std::clamp(pan, -1.0F, 1.0F);
  const float left = clamped <= 0.0F ? 1.0F : 1.0F - clamped;
  const float right = clamped >= 0.0F ? 1.0F : 1.0F + clamped;
  return {left, right};
}

} // namespace Game::Audio
