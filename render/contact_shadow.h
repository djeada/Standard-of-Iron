#pragma once

#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "scene/environment_lighting.h"

namespace Render {

struct ContactShadowPlacement {

  QVector2D direction{0.0F, 1.0F};

  float offset_scale = 1.0F;

  float opacity = 1.0F;
};

struct ContactShadowInputs {
  float camera_distance = 0.0F;
  float fade_distance = 1.0F;
  bool directional_shadows_enabled = false;
  float directional_distance = 0.0F;
};

inline constexpr float k_contact_shadow_min_sun_elevation = 0.12F;
inline constexpr float k_contact_shadow_max_offset_scale = 2.6F;
inline constexpr float k_contact_shadow_grounded_opacity = 0.42F;
inline constexpr float k_contact_shadow_grounded_offset = 0.35F;

[[nodiscard]] inline auto
contact_shadow_fade(float edge0, float edge1, float value) noexcept -> float {
  if (edge1 <= edge0) {
    return value >= edge1 ? 1.0F : 0.0F;
  }
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

[[nodiscard]] inline auto contact_shadow_placement(
    const EnvironmentLightingState& environment,
    const ContactShadowInputs& inputs) noexcept -> ContactShadowPlacement {
  ContactShadowPlacement placement;

  const QVector3D sun = environment.sanitized().primary_direction;
  QVector2D horizontal(sun.x(), sun.z());
  const float horizontal_length = horizontal.length();
  if (horizontal_length > 1e-4F) {

    placement.direction = -(horizontal / horizontal_length);
  }

  const float elevation =
      std::max(std::abs(sun.y()), k_contact_shadow_min_sun_elevation);
  placement.offset_scale = std::clamp(
      horizontal_length / elevation, 0.0F, k_contact_shadow_max_offset_scale);

  const float fade_distance = std::max(inputs.fade_distance, 0.001F);
  placement.opacity =
      1.0F -
      contact_shadow_fade(fade_distance * 0.75F, fade_distance, inputs.camera_distance);

  if (inputs.directional_shadows_enabled && inputs.directional_distance > 0.0F) {

    const float takeover =
        1.0F - contact_shadow_fade(inputs.directional_distance * 0.6F,
                                   inputs.directional_distance,
                                   inputs.camera_distance);
    placement.opacity *= 1.0F + ((k_contact_shadow_grounded_opacity - 1.0F) * takeover);
    placement.offset_scale *=
        1.0F + ((k_contact_shadow_grounded_offset - 1.0F) * takeover);
  }

  placement.opacity = std::clamp(placement.opacity, 0.0F, 1.0F);
  return placement;
}

} // namespace Render
