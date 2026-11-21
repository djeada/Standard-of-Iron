#pragma once

#include <algorithm>
#include <cmath>

namespace Render::Pipeline {

inline constexpr float k_reference_viewport_height_px = 1080.0F;
inline constexpr float k_reference_vertical_fov_degrees = 45.0F;

inline constexpr float k_min_apparent_size_scale = 0.25F;
inline constexpr float k_max_apparent_size_scale = 4.0F;

[[nodiscard]] inline auto
compute_focal_length_px(float vertical_fov_degrees,
                        float viewport_height_px) noexcept -> float {
  constexpr float k_deg_to_rad = 0.017453292519943295F;
  if (!(viewport_height_px > 0.0F)) {
    return 0.0F;
  }
  const float half_angle =
      std::clamp(vertical_fov_degrees, 1.0F, 179.0F) * 0.5F * k_deg_to_rad;
  const float tan_half = std::tan(half_angle);
  if (!(tan_half > 0.0F)) {
    return 0.0F;
  }
  return (viewport_height_px * 0.5F) / tan_half;
}

[[nodiscard]] inline auto reference_focal_length_px() noexcept -> float {
  return compute_focal_length_px(k_reference_vertical_fov_degrees,
                                 k_reference_viewport_height_px);
}

struct ScreenMetrics {

  float focal_length_px{0.0F};

  float size_scale{1.0F};

  [[nodiscard]] static auto
  from_viewport(float vertical_fov_degrees,
                int viewport_height_px) noexcept -> ScreenMetrics {
    const float focal = compute_focal_length_px(
        vertical_fov_degrees, static_cast<float>(std::max(0, viewport_height_px)));
    if (!(focal > 0.0F)) {
      return {};
    }
    return ScreenMetrics{.focal_length_px = focal,
                         .size_scale = std::clamp(focal / reference_focal_length_px(),
                                                  k_min_apparent_size_scale,
                                                  k_max_apparent_size_scale)};
  }

  [[nodiscard]] auto known() const noexcept -> bool { return focal_length_px > 0.0F; }

  [[nodiscard]] auto apparent_size_scale() const noexcept -> float {
    return size_scale;
  }

  [[nodiscard]] auto projected_radius_px(float distance,
                                         float world_radius) const noexcept -> float {
    if (!known() || !(world_radius > 0.0F)) {
      return -1.0F;
    }
    if (!(distance > 0.0F)) {
      return focal_length_px;
    }
    return focal_length_px * world_radius / distance;
  }

  [[nodiscard]] auto
  projected_radius_px_from_distance_sq(float distance_sq,
                                       float world_radius) const noexcept -> float {
    if (!known() || !(world_radius > 0.0F)) {
      return -1.0F;
    }
    if (!(distance_sq > 0.0F)) {
      return focal_length_px;
    }
    return focal_length_px * world_radius / std::sqrt(distance_sq);
  }

  [[nodiscard]] auto reference_distance(float distance) const noexcept -> float {
    return size_scale > 0.0F ? distance / size_scale : distance;
  }

  [[nodiscard]] auto reference_distance_sq(float distance_sq) const noexcept -> float {
    return size_scale > 0.0F ? distance_sq / (size_scale * size_scale) : distance_sq;
  }
};

} // namespace Render::Pipeline
