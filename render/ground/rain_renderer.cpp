#include "rain_renderer.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "ground_utils.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

RainRenderer::RainRenderer() = default;
RainRenderer::~RainRenderer() = default;

void RainRenderer::configure(float world_width, float world_height,
                             std::uint32_t seed, Game::Map::WeatherType type) {
  m_world_width = std::max(1.0F, world_width);
  m_world_height = std::max(1.0F, world_height);
  m_seed = seed;

  m_rain_drops.clear();
  m_instance_buffer.reset();
  m_instance_count = 0;

  m_params.weather_type = type;
  m_params.time = 0.0F;
  m_params.intensity = 0.0F;
  m_params.wind_strength = 0.0F;

  update_weather_params();
  generate_rain_drops();
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
  m_params.wind_strength = strength;
}

void RainRenderer::set_camera_position(const QVector3D &position) {
  m_camera_position = position;
}

void RainRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  if (!m_enabled) {
    return;
  }

  const float delta_time = 0.016F;
  if (m_intensity < m_target_intensity) {
    m_intensity = std::min(m_target_intensity,
                           m_intensity + delta_time * k_intensity_lerp_speed);
  } else if (m_intensity > m_target_intensity) {
    m_intensity = std::max(m_target_intensity,
                           m_intensity - delta_time * k_intensity_lerp_speed);
  }

  if (m_intensity < 0.001F) {
    return;
  }

  const float time = renderer.get_animation_time();

  const auto visible_count = static_cast<std::size_t>(
      static_cast<float>(m_rain_drops.size()) * m_intensity);

  if (visible_count == 0) {
    return;
  }

  if (!m_instance_buffer) {
    m_instance_buffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
  }

  m_instance_buffer->set_data(m_rain_drops.data(),
                              visible_count * sizeof(RainDropInstanceGpu),
                              Buffer::Usage::Dynamic);

  m_params.time = time;
  m_params.intensity = m_intensity;

  renderer.rain_batch(m_instance_buffer.get(), visible_count, m_params);
}

void RainRenderer::clear() {
  m_rain_drops.clear();
  m_instance_buffer.reset();
  m_instance_count = 0;
  m_intensity = 0.0F;
  m_target_intensity = 0.0F;
}

void RainRenderer::generate_rain_drops() {
  m_rain_drops.clear();

  const std::size_t max_drops =
      (m_params.weather_type == Game::Map::WeatherType::Snow)
          ? k_max_snow_drops
          : k_max_rain_drops;

  m_rain_drops.reserve(max_drops);

  uint32_t state = m_seed;

  for (std::size_t i = 0; i < max_drops; ++i) {
    state = hash_coords(static_cast<int>(i), static_cast<int>(i * 17),
                        m_seed ^ 0xDA1A1234U);

    const float x = (rand_01(state) - 0.5F) * m_rain_area_radius * 2.0F;
    state = hash_coords(static_cast<int>(i * 31), static_cast<int>(i), state);
    const float z = (rand_01(state) - 0.5F) * m_rain_area_radius * 2.0F;
    state =
        hash_coords(static_cast<int>(i * 7), static_cast<int>(i * 13), state);
    const float y = rand_01(state) * m_rain_height;

    state = hash_coords(static_cast<int>(i), static_cast<int>(i * 23), state);

    float speed_variation;
    if (m_params.weather_type == Game::Map::WeatherType::Snow) {

      speed_variation =
          RainBatchParams::k_snow_speed_variation_min +
          rand_01(state) * RainBatchParams::k_snow_speed_variation_range;
    } else {

      speed_variation =
          RainBatchParams::k_rain_speed_variation_min +
          rand_01(state) * RainBatchParams::k_rain_speed_variation_range;
    }

    RainDropInstanceGpu drop;
    drop.pos_velocity =
        QVector4D(x, y, z, m_params.drop_speed * speed_variation);
    drop.size_alpha =
        QVector4D(m_params.drop_length, m_params.drop_width, 1.0F, 0.0F);

    m_rain_drops.push_back(drop);
  }

  m_instance_count = m_rain_drops.size();
}

} // namespace Render::GL
