#pragma once

#include <algorithm>
#include <utility>

#include "scene/camera.h"

namespace Render::GL {

inline constexpr float k_fog_start_zoom_scale = 1.25F;
inline constexpr float k_fog_end_zoom_scale = 3.2F;

[[nodiscard]] inline auto
fog_range_for_camera(const Camera& camera) -> std::pair<float, float> {
  const float orbit_distance =
      std::max((camera.get_position() - camera.get_target()).length(), 1.0F);
  const float near_fog_start =
      std::max(camera.get_near() + 5.0F, camera.get_far() * 0.18F);
  const float near_fog_end = std::max(near_fog_start + 1.0F, camera.get_far() * 0.62F);
  return {std::max(near_fog_start, orbit_distance * k_fog_start_zoom_scale),
          std::max(near_fog_end, orbit_distance * k_fog_end_zoom_scale)};
}

} // namespace Render::GL
