#include "rain_pipeline.h"
#include "../backend.h"
#include "../camera.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include <QDebug>
#include <QOpenGLContext>
#include <cmath>
#include <numbers>
#include <random>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {
constexpr float kRainColorR = 0.7F;
constexpr float kRainColorG = 0.75F;
constexpr float kRainColorB = 0.85F;

void clear_gl_errors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

auto check_gl_error(const char *operation) -> bool {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "RainPipeline GL error in" << operation << ":" << err;
    return false;
  }
  return true;
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
    m_rain_shader =
        m_shader_cache->load("rain", QStringLiteral(":/assets/shaders/rain.vert"),
                             QStringLiteral(":/assets/shaders/rain.frag"));
  }
  if (m_rain_shader == nullptr) {
    qWarning() << "RainPipeline: Failed to get rain shader";
    return false;
  }

  cache_uniforms();
  generate_rain_drops();

  if (!create_rain_geometry()) {
    qWarning() << "RainPipeline: Failed to create rain geometry";
    return false;
  }

  qInfo() << "RainPipeline initialized successfully";
  return is_initialized();
}

void RainPipeline::shutdown() {
  shutdown_geometry();
  m_rain_shader = nullptr;
  m_rain_drops.clear();
}

void RainPipeline::shutdown_geometry() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_vao = 0;
    m_vertex_buffer = 0;
    m_index_buffer = 0;
    m_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
  if (m_vertex_buffer != 0) {
    glDeleteBuffers(1, &m_vertex_buffer);
    m_vertex_buffer = 0;
  }
  if (m_index_buffer != 0) {
    glDeleteBuffers(1, &m_index_buffer);
    m_index_buffer = 0;
  }
  m_index_count = 0;
}

void RainPipeline::cache_uniforms() {
  if (m_rain_shader == nullptr) {
    return;
  }

  m_uniforms.view_proj = m_rain_shader->uniform_handle("u_view_proj");
  m_uniforms.time = m_rain_shader->uniform_handle("u_time");
  m_uniforms.intensity = m_rain_shader->uniform_handle("u_intensity");
  m_uniforms.camera_pos = m_rain_shader->uniform_handle("u_camera_pos");
  m_uniforms.rain_color = m_rain_shader->uniform_handle("u_rain_color");
  m_uniforms.wind = m_rain_shader->uniform_handle("u_wind");
}

auto RainPipeline::is_initialized() const -> bool {
  return m_rain_shader != nullptr && m_vao != 0 && m_index_count > 0;
}

void RainPipeline::generate_rain_drops() {
  m_rain_drops.clear();
  m_rain_drops.reserve(k_max_drops);

  std::mt19937 rng(42);
  std::uniform_real_distribution<float> dist_xz(-k_area_radius, k_area_radius);
  std::uniform_real_distribution<float> dist_y(0.0F, k_area_height);
  std::uniform_real_distribution<float> dist_speed(0.8F, 1.2F);
  std::uniform_real_distribution<float> dist_alpha(0.3F, 0.7F);

  for (std::size_t i = 0; i < k_max_drops; ++i) {
    RainDropData drop;
    drop.position = QVector3D(dist_xz(rng), dist_y(rng), dist_xz(rng));
    drop.speed = k_drop_speed * dist_speed(rng);
    drop.length = k_drop_length;
    drop.alpha = dist_alpha(rng);
    m_rain_drops.push_back(drop);
  }
}

struct RainVertex {
  float position[3];
  float offset[3];
  float alpha;
};

auto RainPipeline::create_rain_geometry() -> bool {
  initializeOpenGLFunctions();
  shutdown_geometry();
  clear_gl_errors();

  std::vector<RainVertex> vertices;
  std::vector<unsigned int> indices;

  vertices.reserve(m_rain_drops.size() * 2);
  indices.reserve(m_rain_drops.size() * 2);

  for (std::size_t i = 0; i < m_rain_drops.size(); ++i) {
    const auto &drop = m_rain_drops[i];

    RainVertex top;
    top.position[0] = drop.position.x();
    top.position[1] = drop.position.y();
    top.position[2] = drop.position.z();
    top.offset[0] = 0.0F;
    top.offset[1] = 0.0F;
    top.offset[2] = drop.speed;
    top.alpha = drop.alpha;
    vertices.push_back(top);

    RainVertex bottom;
    bottom.position[0] = drop.position.x();
    bottom.position[1] = drop.position.y() - drop.length;
    bottom.position[2] = drop.position.z();
    bottom.offset[0] = 0.0F;
    bottom.offset[1] = -drop.length;
    bottom.offset[2] = drop.speed;
    bottom.alpha = drop.alpha * 0.3F;
    vertices.push_back(bottom);

    auto base = static_cast<unsigned int>(i * 2);
    indices.push_back(base);
    indices.push_back(base + 1);
  }

  glGenVertexArrays(1, &m_vao);
  if (!check_gl_error("glGenVertexArrays") || m_vao == 0) {
    return false;
  }

  glBindVertexArray(m_vao);
  if (!check_gl_error("glBindVertexArray")) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
    return false;
  }

  glGenBuffers(1, &m_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(RainVertex)),
               vertices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("vertex buffer")) {
    shutdown_geometry();
    return false;
  }

  glGenBuffers(1, &m_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("index buffer")) {
    shutdown_geometry();
    return false;
  }

  m_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(VertexAttrib::Position);
  glVertexAttribPointer(
      VertexAttrib::Position, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      sizeof(RainVertex),
      reinterpret_cast<void *>(offsetof(RainVertex, position)));

  glEnableVertexAttribArray(VertexAttrib::Normal);
  glVertexAttribPointer(VertexAttrib::Normal, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(RainVertex),
                        reinterpret_cast<void *>(offsetof(RainVertex, offset)));

  glEnableVertexAttribArray(VertexAttrib::TexCoord);
  glVertexAttribPointer(VertexAttrib::TexCoord, 1, GL_FLOAT, GL_FALSE,
                        sizeof(RainVertex),
                        reinterpret_cast<void *>(offsetof(RainVertex, alpha)));

  glBindVertexArray(0);

  if (!check_gl_error("vertex attributes")) {
    shutdown_geometry();
    return false;
  }

  return true;
}

void RainPipeline::render(const Camera &cam, float intensity, float time) {
  if (!is_initialized() || intensity < 0.01F) {
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);
  GLboolean blend_enabled = glIsEnabled(GL_BLEND);
  GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_rain_shader->use();
  glBindVertexArray(m_vao);

  QMatrix4x4 view_proj = cam.get_projection_matrix() * cam.get_view_matrix();
  QVector3D camera_pos = cam.get_position();
  QVector3D rain_color(kRainColorR, kRainColorG, kRainColorB);

  m_rain_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_rain_shader->set_uniform(m_uniforms.time, time);
  m_rain_shader->set_uniform(m_uniforms.intensity, intensity);
  m_rain_shader->set_uniform(m_uniforms.camera_pos, camera_pos);
  m_rain_shader->set_uniform(m_uniforms.rain_color, rain_color);
  m_rain_shader->set_uniform(m_uniforms.wind, m_wind_direction);

  glDrawElements(GL_LINES, m_index_count, GL_UNSIGNED_INT, nullptr);

  glBindVertexArray(0);

  glDepthMask(depth_mask_enabled);
  if (!blend_enabled) {
    glDisable(GL_BLEND);
  }
  if (!depth_test_enabled) {
    glDisable(GL_DEPTH_TEST);
  }
}

} // namespace Render::GL::BackendPipelines
