#include "rain_renderer.h"

#include <algorithm>

#include "../graphics_settings.h"
#include "../scene_renderer.h"

namespace Render::GL {

RainRenderer::RainRenderer() = default;
RainRenderer::~RainRenderer() = default;

void RainRenderer::configure(float world_width,
                             float world_height,
                             std::uint32_t seed,
                             Game::Map::WeatherType type) {
  m_world_width = std::max(1.0F, world_width);
  m_world_height = std::max(1.0F, world_height);
  m_seed = seed;

  m_params.weather_type = type;
  m_params.time = 0.0F;
  m_params.intensity = 0.0F;
  m_params.wind_strength = 0.0F;
  m_params.density = 0.0F;

  update_weather_params();
}

void RainRenderer::set_intensity(float intensity) {
  m_target_intensity = std::clamp(intensity, 0.0F, 1.0F);
}

void RainRenderer::set_weather_type(Game::Map::WeatherType type) {
  if (m_params.weather_type != type) {
    m_params.weather_type = type;
    update_weather_params();
  }
}

void RainRenderer::update_weather_params() {
  if (m_params.weather_type == Game::Map::WeatherType::Snow) {
    m_params.drop_speed = RainBatchParams::k_default_snow_drop_speed;
    m_params.drop_length = RainBatchParams::k_default_snow_drop_size;
    m_params.drop_width = RainBatchParams::k_default_snow_drop_size;
  } else {
    m_params.drop_speed = RainBatchParams::k_default_rain_drop_speed;
    m_params.drop_length = RainBatchParams::k_default_rain_drop_length;
    m_params.drop_width = RainBatchParams::k_default_rain_drop_width;
  }
}

void RainRenderer::set_wind_strength(float strength) {
  m_params.wind_strength = std::max(0.0F, strength);
}

void RainRenderer::set_wind_direction_deg(float degrees) {
  m_params.wind_direction = Game::Map::weather_wind_vector(degrees);
}

void RainRenderer::set_camera_position(const QVector3D& position) {
  m_camera_position = position;
}

void RainRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  (void)resources;

  if (!m_enabled) {
    return;
  }

  const float now = renderer.get_animation_time();
  const float delta_time = m_last_submit_time < 0.0F
                               ? 0.0F
                               : std::clamp(now - m_last_submit_time, 0.0F, 0.5F);
  m_last_submit_time = now;

  if (m_intensity < m_target_intensity) {
    m_intensity =
        std::min(m_target_intensity, m_intensity + delta_time * k_intensity_lerp_speed);
  } else if (m_intensity > m_target_intensity) {
    m_intensity =
        std::max(m_target_intensity, m_intensity - delta_time * k_intensity_lerp_speed);
  }

  if (m_intensity < 0.001F) {
    return;
  }

  m_params.time = now;
  m_params.intensity = m_intensity;
  m_params.density = weather_particle_density(
      {.intensity = m_intensity,
       .quality_scale = GraphicsSettings::instance().weather_budget().particle_scale,
       .camera_height = m_camera_position.y()});

  if (m_params.density < 0.001F) {
    return;
  }

  renderer.rain_batch(m_params);
}

void RainRenderer::clear() {
  m_intensity = 0.0F;
  m_target_intensity = 0.0F;
  m_params.density = 0.0F;
  m_last_submit_time = -1.0F;
}

} // namespace Render::GL
