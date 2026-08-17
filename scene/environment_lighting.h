#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Render {

struct EnvironmentLightingState {
  static constexpr std::size_t k_packed_float_count = 28;

  QVector3D primary_direction{0.35F, 0.85F, 0.42F};
  QVector3D primary_color{1.0F, 0.95F, 0.86F};
  float primary_intensity = 1.0F;

  QVector3D sky_color{0.50F, 0.62F, 0.78F};
  QVector3D ground_bounce_color{0.24F, 0.20F, 0.15F};
  float ambient_intensity = 0.30F;

  QVector3D fog_color{0.58F, 0.65F, 0.72F};
  float fog_density = 0.0F;

  QVector3D shadow_tint{0.30F, 0.34F, 0.44F};
  float shadow_strength = 0.60F;
  float shadow_softness = 0.35F;

  float exposure = 1.0F;
  float cloud_cover = 0.0F;
  float wetness = 0.0F;

  [[nodiscard]] auto sanitized() const noexcept -> EnvironmentLightingState {
    EnvironmentLightingState result = *this;
    result.primary_direction = primary_direction.isNull()
                                   ? QVector3D(0.35F, 0.85F, 0.42F).normalized()
                                   : primary_direction.normalized();
    result.primary_intensity = std::max(0.0F, primary_intensity);
    result.ambient_intensity = std::max(0.0F, ambient_intensity);
    result.fog_density = std::max(0.0F, fog_density);
    result.shadow_strength = std::clamp(shadow_strength, 0.0F, 1.0F);
    result.shadow_softness = std::clamp(shadow_softness, 0.0F, 1.0F);
    result.exposure = std::max(0.01F, exposure);
    result.cloud_cover = std::clamp(cloud_cover, 0.0F, 1.0F);
    result.wetness = std::clamp(wetness, 0.0F, 1.0F);
    return result;
  }

  [[nodiscard]] auto
  packed_std140() const noexcept -> std::array<float, k_packed_float_count> {
    const EnvironmentLightingState value = sanitized();
    return {
        value.primary_direction.x(),
        value.primary_direction.y(),
        value.primary_direction.z(),
        value.primary_intensity,
        value.primary_color.x(),
        value.primary_color.y(),
        value.primary_color.z(),
        value.ambient_intensity,
        value.sky_color.x(),
        value.sky_color.y(),
        value.sky_color.z(),
        value.exposure,
        value.ground_bounce_color.x(),
        value.ground_bounce_color.y(),
        value.ground_bounce_color.z(),
        value.fog_density,
        value.fog_color.x(),
        value.fog_color.y(),
        value.fog_color.z(),
        value.cloud_cover,
        value.shadow_tint.x(),
        value.shadow_tint.y(),
        value.shadow_tint.z(),
        value.shadow_strength,
        value.shadow_softness,
        value.wetness,
        0.0F,
        0.0F,
    };
  }
};

[[nodiscard]] inline auto
environment_night_amount(const EnvironmentLightingState& state) noexcept -> float {
  const float blueness = state.primary_color.z() - state.primary_color.x();
  const float t = std::clamp((blueness - 0.05F) / 0.35F, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] inline auto
interpolate_environment_lighting(const EnvironmentLightingState& from,
                                 const EnvironmentLightingState& to,
                                 float amount) noexcept -> EnvironmentLightingState {
  const float t = std::clamp(amount, 0.0F, 1.0F);
  const auto mix_float = [t](float a, float b) {
    return a + ((b - a) * t);
  };
  const auto mix_vec = [t](const QVector3D& a, const QVector3D& b) {
    return a + ((b - a) * t);
  };

  EnvironmentLightingState result;
  result.primary_direction =
      mix_vec(from.primary_direction, to.primary_direction).normalized();
  result.primary_color = mix_vec(from.primary_color, to.primary_color);
  result.primary_intensity = mix_float(from.primary_intensity, to.primary_intensity);
  result.sky_color = mix_vec(from.sky_color, to.sky_color);
  result.ground_bounce_color =
      mix_vec(from.ground_bounce_color, to.ground_bounce_color);
  result.ambient_intensity = mix_float(from.ambient_intensity, to.ambient_intensity);
  result.fog_color = mix_vec(from.fog_color, to.fog_color);
  result.fog_density = mix_float(from.fog_density, to.fog_density);
  result.shadow_tint = mix_vec(from.shadow_tint, to.shadow_tint);
  result.shadow_strength = mix_float(from.shadow_strength, to.shadow_strength);
  result.shadow_softness = mix_float(from.shadow_softness, to.shadow_softness);
  result.exposure = mix_float(from.exposure, to.exposure);
  result.cloud_cover = mix_float(from.cloud_cover, to.cloud_cover);
  result.wetness = mix_float(from.wetness, to.wetness);
  return result.sanitized();
}

} // namespace Render
