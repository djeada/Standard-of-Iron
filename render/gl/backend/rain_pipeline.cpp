#include "rain_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>

#include "../../rain_gpu.h"
#include "../backend.h"
#include "../shader_cache.h"
#include "gl_error_check.h"
#include "scene/camera.h"

namespace Render::GL::BackendPipelines {

namespace {

auto check_gl_error(const char* operation) -> bool {
  return BackendPipelines::check_gl_error("RainPipeline", operation);
}

struct WeatherLook {
  float fall_speed = 1.0F;
  float streak_half_length = 0.0F;
  float particle_half_size = 0.05F;
  QVector2D speed_range{1.0F, 0.0F};
  QVector2D size_range{1.0F, 0.0F};
  QVector2D alpha_range{0.5F, 0.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};
  float pool_fraction = 1.0F;
};

constexpr int k_corner_attrib = 0;
constexpr int k_seed_attrib = 1;
constexpr int k_props_attrib = 2;

constexpr GLsizei k_quad_index_count = 6;

constexpr float k_field_radius_base = 20.0F;
constexpr float k_field_radius_per_height = 0.5F;
constexpr float k_field_radius_min = 26.0F;
constexpr float k_field_radius_max = 55.0F;

constexpr float k_field_height_base = 18.0F;
constexpr float k_field_height_per_height = 0.6F;
constexpr float k_field_height_min = 24.0F;
constexpr float k_field_height_max = 55.0F;

constexpr float k_rain_sky_blend = 0.55F;
constexpr float k_snow_sky_blend = 0.35F;
constexpr float k_brightness_floor = 0.55F;
constexpr float k_brightness_ceiling = 1.15F;

constexpr QVector3D k_rain_base_color{0.62F, 0.68F, 0.78F};
constexpr QVector3D k_snow_base_color{0.94F, 0.96F, 1.0F};

auto rain_look() -> WeatherLook {
  WeatherLook look;
  look.speed_range = QVector2D(0.85F, 0.45F);
  look.size_range = QVector2D(0.7F, 0.7F);
  look.alpha_range = QVector2D(0.16F, 0.34F);
  look.color = k_rain_base_color;
  look.pool_fraction = 1.0F;
  return look;
}

auto snow_look() -> WeatherLook {
  WeatherLook look;
  look.speed_range = QVector2D(0.55F, 0.75F);
  look.size_range = QVector2D(0.75F, 0.5F);
  look.alpha_range = QVector2D(0.3F, 0.45F);
  look.color = k_snow_base_color;
  look.pool_fraction = 0.75F;
  return look;
}

} // namespace

auto RainPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "RainPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_rain_shader = m_shader_cache->get("rain");
  if (m_rain_shader == nullptr) {
    m_rain_shader = m_shader_cache->load("rain",
                                         QStringLiteral(":/assets/shaders/rain.vert"),
                                         QStringLiteral(":/assets/shaders/rain.frag"));
  }
  if (m_rain_shader == nullptr) {
    qWarning() << "RainPipeline: Failed to get rain shader";
    return false;
  }

  cache_uniforms();
  generate_particles();

  if (!create_geometry()) {
    qWarning() << "RainPipeline: Failed to create weather geometry";
    return false;
  }

  qInfo() << "RainPipeline initialized successfully";
  return is_initialized();
}

void RainPipeline::shutdown() {
  shutdown_geometry();
  m_rain_shader = nullptr;
  m_particles.clear();
}

void RainPipeline::shutdown_geometry() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_vao = 0;
    m_quad_buffer = 0;
    m_index_buffer = 0;
    m_instance_buffer = 0;
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
  if (m_quad_buffer != 0) {
    glDeleteBuffers(1, &m_quad_buffer);
    m_quad_buffer = 0;
  }
  if (m_index_buffer != 0) {
    glDeleteBuffers(1, &m_index_buffer);
    m_index_buffer = 0;
  }
  if (m_instance_buffer != 0) {
    glDeleteBuffers(1, &m_instance_buffer);
    m_instance_buffer = 0;
  }
}

void RainPipeline::cache_uniforms() {
  if (m_rain_shader == nullptr) {
    return;
  }

  m_uniforms.view_proj = m_rain_shader->uniform_handle("u_view_proj");
  m_uniforms.camera_pos = m_rain_shader->uniform_handle("u_camera_pos");
  m_uniforms.camera_right = m_rain_shader->uniform_handle("u_camera_right");
  m_uniforms.camera_up = m_rain_shader->uniform_handle("u_camera_up");
  m_uniforms.time = m_rain_shader->uniform_handle("u_time");
  m_uniforms.intensity = m_rain_shader->uniform_handle("u_intensity");
  m_uniforms.density = m_rain_shader->uniform_handle("u_density");
  m_uniforms.rank_step = m_rain_shader->uniform_handle("u_rank_step");
  m_uniforms.weather_type = m_rain_shader->uniform_handle("u_weather_type");
  m_uniforms.wind = m_rain_shader->uniform_handle("u_wind");
  m_uniforms.wind_strength = m_rain_shader->uniform_handle("u_wind_strength");
  m_uniforms.fall_speed = m_rain_shader->uniform_handle("u_fall_speed");
  m_uniforms.streak_half_length = m_rain_shader->uniform_handle("u_streak_half_length");
  m_uniforms.particle_half_size = m_rain_shader->uniform_handle("u_particle_half_size");
  m_uniforms.field = m_rain_shader->uniform_handle("u_field");
  m_uniforms.pixel_scale = m_rain_shader->uniform_handle("u_pixel_scale");
  m_uniforms.speed_range = m_rain_shader->uniform_handle("u_speed_range");
  m_uniforms.size_range = m_rain_shader->uniform_handle("u_size_range");
  m_uniforms.alpha_range = m_rain_shader->uniform_handle("u_alpha_range");
  m_uniforms.rain_color = m_rain_shader->uniform_handle("u_rain_color");
}

auto RainPipeline::is_initialized() const -> bool {
  return m_rain_shader != nullptr && m_vao != 0 && m_instance_buffer != 0;
}

void RainPipeline::generate_particles() {
  m_particles.clear();
  m_particles.reserve(k_max_particles);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> unit(0.0F, 1.0F);
  std::uniform_real_distribution<float> signed_unit(-1.0F, 1.0F);
  std::uniform_real_distribution<float> phase(0.0F, 2.0F * std::numbers::pi_v<float>);
  std::uniform_real_distribution<float> spin(-1.4F, 1.4F);

  for (std::size_t i = 0; i < k_max_particles; ++i) {
    WeatherParticleGpu particle{};

    particle.seed[0] = signed_unit(rng);
    particle.seed[1] = unit(rng);
    particle.seed[2] = signed_unit(rng);
    particle.seed[3] = unit(rng);

    particle.props[0] = unit(rng);
    particle.props[1] = unit(rng);
    particle.props[2] = phase(rng);
    particle.props[3] = spin(rng);

    m_particles.push_back(particle);
  }
}

auto RainPipeline::create_geometry() -> bool {
  initializeOpenGLFunctions();
  shutdown_geometry();
  clear_gl_errors();

  constexpr std::array<float, 8> quad_corners{
      -1.0F, -1.0F, 1.0F, -1.0F, 1.0F, 1.0F, -1.0F, 1.0F};
  constexpr std::array<unsigned int, k_quad_index_count> quad_indices{0, 1, 2, 0, 2, 3};

  glGenVertexArrays(1, &m_vao);
  if (!check_gl_error("glGenVertexArrays") || m_vao == 0) {
    return false;
  }
  glBindVertexArray(m_vao);

  glGenBuffers(1, &m_quad_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_quad_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(quad_corners.size() * sizeof(float)),
               quad_corners.data(),
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(k_corner_attrib);
  glVertexAttribPointer(
      k_corner_attrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

  glGenBuffers(1, &m_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(quad_indices.size() * sizeof(unsigned int)),
               quad_indices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_instance_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_particles.size() * sizeof(WeatherParticleGpu)),
               m_particles.data(),
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(k_seed_attrib);
  glVertexAttribPointer(k_seed_attrib,
                        4,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(WeatherParticleGpu),
                        reinterpret_cast<void*>(offsetof(WeatherParticleGpu, seed)));
  glVertexAttribDivisor(k_seed_attrib, 1);

  glEnableVertexAttribArray(k_props_attrib);
  glVertexAttribPointer(k_props_attrib,
                        4,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(WeatherParticleGpu),
                        reinterpret_cast<void*>(offsetof(WeatherParticleGpu, props)));
  glVertexAttribDivisor(k_props_attrib, 1);

  glBindVertexArray(0);

  if (!check_gl_error("weather geometry")) {
    shutdown_geometry();
    return false;
  }

  return true;
}

void RainPipeline::render(const Camera& cam, const RainBatchParams& params) {
  if (!is_initialized() || params.intensity < 0.01F) {
    return;
  }

  const bool is_snow = params.weather_type == Game::Map::WeatherType::Snow;
  WeatherLook look = is_snow ? snow_look() : rain_look();
  look.fall_speed = params.drop_speed;
  look.streak_half_length = params.drop_length;
  look.particle_half_size = params.drop_width;

  const float density = std::clamp(params.density * look.pool_fraction, 0.0F, 1.0F);
  const auto instance_count =
      static_cast<GLsizei>(std::ceil(static_cast<float>(k_max_particles) * density));
  if (instance_count <= 0) {
    return;
  }

  clear_gl_errors();

  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);
  GLboolean blend_enabled = glIsEnabled(GL_BLEND);
  GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glDisable(GL_CULL_FACE);

  m_rain_shader->use();
  glBindVertexArray(m_vao);

  const QVector3D camera_pos = cam.get_position();
  const float camera_height = std::max(0.0F, camera_pos.y());

  const float field_radius =
      std::clamp(k_field_radius_base + (camera_height * k_field_radius_per_height),
                 k_field_radius_min,
                 k_field_radius_max);
  const float field_height =
      std::clamp(k_field_height_base + (camera_height * k_field_height_per_height),
                 k_field_height_min,
                 k_field_height_max);

  const int viewport_height = m_backend != nullptr ? m_backend->viewport_height() : 0;
  const float half_fov_tan =
      std::tan(0.5F * cam.get_fov() * std::numbers::pi_v<float> / 180.0F);
  const float pixel_scale =
      viewport_height > 0 ? (2.0F * half_fov_tan / static_cast<float>(viewport_height))
                          : 0.0F;

  QVector3D wind = params.wind_direction;
  wind.setY(0.0F);
  wind = wind.lengthSquared() > 1e-6F ? wind.normalized() : QVector3D(1.0F, 0.0F, 0.0F);

  QVector3D particle_color = look.color;
  if (m_backend != nullptr) {
    const auto& lighting = m_backend->environment_lighting();
    const float sky_blend = is_snow ? k_snow_sky_blend : k_rain_sky_blend;
    particle_color += (lighting.sky_color - particle_color) * sky_blend;
    const float brightness =
        std::clamp(k_brightness_floor + lighting.ambient_intensity +
                       (lighting.primary_intensity * 0.25F),
                   k_brightness_floor,
                   k_brightness_ceiling);
    particle_color *= brightness;
    particle_color = QVector3D(std::min(particle_color.x(), 1.0F),
                               std::min(particle_color.y(), 1.0F),
                               std::min(particle_color.z(), 1.0F));
  }

  m_rain_shader->set_uniform(m_uniforms.view_proj, cam.get_view_projection_matrix());
  m_rain_shader->set_uniform(m_uniforms.camera_pos, camera_pos);
  m_rain_shader->set_uniform(m_uniforms.camera_right, cam.get_right_vector());
  m_rain_shader->set_uniform(m_uniforms.camera_up, cam.get_up_vector());
  m_rain_shader->set_uniform(m_uniforms.time, params.time);
  m_rain_shader->set_uniform(m_uniforms.intensity, params.intensity);
  m_rain_shader->set_uniform(m_uniforms.density, density);
  m_rain_shader->set_uniform(m_uniforms.rank_step,
                             1.0F / static_cast<float>(k_max_particles));
  m_rain_shader->set_uniform(m_uniforms.weather_type,
                             static_cast<int>(params.weather_type));
  m_rain_shader->set_uniform(m_uniforms.wind, wind);
  m_rain_shader->set_uniform(m_uniforms.wind_strength, params.wind_strength);
  m_rain_shader->set_uniform(m_uniforms.fall_speed, look.fall_speed);
  m_rain_shader->set_uniform(m_uniforms.streak_half_length, look.streak_half_length);
  m_rain_shader->set_uniform(m_uniforms.particle_half_size, look.particle_half_size);
  m_rain_shader->set_uniform(m_uniforms.field, QVector2D(field_radius, field_height));
  m_rain_shader->set_uniform(m_uniforms.pixel_scale, pixel_scale);
  m_rain_shader->set_uniform(m_uniforms.speed_range, look.speed_range);
  m_rain_shader->set_uniform(m_uniforms.size_range, look.size_range);
  m_rain_shader->set_uniform(m_uniforms.alpha_range, look.alpha_range);
  m_rain_shader->set_uniform(m_uniforms.rain_color, particle_color);

  glDrawElementsInstanced(
      GL_TRIANGLES,
      k_quad_index_count,
      GL_UNSIGNED_INT,
      nullptr,
      std::min(instance_count, static_cast<GLsizei>(k_max_particles)));

  glBindVertexArray(0);

  glDepthMask(depth_mask_enabled);
  if (!blend_enabled) {
    glDisable(GL_BLEND);
  }
  if (!depth_test_enabled) {
    glDisable(GL_DEPTH_TEST);
  }
  if (cull_enabled) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
