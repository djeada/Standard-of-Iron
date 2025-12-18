#pragma once

#include "../../game/map/map_definition.h"
#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

struct RainDropInstanceGpu {
  QVector4D pos_velocity;
  QVector4D size_alpha;
};

struct RainBatchParams {
  static constexpr float k_default_rain_drop_speed = 20.0F;
  static constexpr float k_default_rain_drop_length = 1.2F;
  static constexpr float k_default_rain_drop_width = 0.025F;

  static constexpr float k_default_snow_drop_speed = 0.1F;
  static constexpr float k_default_snow_drop_size = 0.08F;

  static constexpr float k_rain_speed_variation_min = 0.8F;
  static constexpr float k_rain_speed_variation_range = 0.4F;
  static constexpr float k_snow_speed_variation_min = 0.6F;
  static constexpr float k_snow_speed_variation_range = 0.8F;

  Game::Map::WeatherType weather_type = Game::Map::WeatherType::Rain;
  float time = 0.0F;
  float intensity = 0.5F;
  float drop_speed = k_default_rain_drop_speed;
  float drop_length = k_default_rain_drop_length;
  float drop_width = k_default_rain_drop_width;
  float wind_strength = 0.0F;
  QVector3D wind_direction{1.0F, 0.0F, 0.3F};
};

} 
