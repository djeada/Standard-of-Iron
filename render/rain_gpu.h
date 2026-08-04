#pragma once

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cstdint>

#include "../game/map/map_definition.h"

namespace Render::GL {

struct RainBatchParams {

  static constexpr float k_default_rain_drop_speed = 22.0F;

  static constexpr float k_default_rain_drop_length = 0.34F;

  static constexpr float k_default_rain_drop_width = 0.018F;

  static constexpr float k_default_snow_drop_speed = 1.8F;

  static constexpr float k_default_snow_drop_size = 0.09F;

  Game::Map::WeatherType weather_type = Game::Map::WeatherType::Rain;
  float time = 0.0F;
  float intensity = 0.5F;
  float drop_speed = k_default_rain_drop_speed;
  float drop_length = k_default_rain_drop_length;
  float drop_width = k_default_rain_drop_width;
  float wind_strength = 0.0F;
  QVector3D wind_direction{1.0F, 0.0F, 0.3F};

  float density = 1.0F;
};

inline constexpr float k_weather_density_fade_band = 0.12F;
inline constexpr float k_weather_density_near_height = 40.0F;
inline constexpr float k_weather_density_far_height = 160.0F;
inline constexpr float k_weather_density_far_scale = 0.45F;

struct WeatherDensityInputs {
  float intensity = 0.0F;
  float quality_scale = 1.0F;
  float camera_height = 0.0F;
};

[[nodiscard]] inline auto
weather_particle_density(const WeatherDensityInputs& inputs) noexcept -> float {
  const float intensity = std::clamp(inputs.intensity, 0.0F, 1.0F);
  const float quality = std::clamp(inputs.quality_scale, 0.0F, 1.0F);

  const float span = k_weather_density_far_height - k_weather_density_near_height;
  const float travelled = std::clamp(
      (inputs.camera_height - k_weather_density_near_height) / span, 0.0F, 1.0F);
  const float distance_scale =
      1.0F + ((k_weather_density_far_scale - 1.0F) * travelled);

  return std::clamp(intensity * quality * distance_scale, 0.0F, 1.0F);
}

} // namespace Render::GL
