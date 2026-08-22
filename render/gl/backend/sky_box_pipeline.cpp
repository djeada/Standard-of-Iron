#include "sky_box_pipeline.h"

#include <QOpenGLContext>
#include <QString>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cstdint>

#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

namespace {

constexpr std::array<float, 24> k_cube_corners{
    -1.0F, -1.0F, -1.0F, 1.0F, -1.0F, -1.0F, 1.0F, 1.0F, -1.0F, -1.0F, 1.0F, -1.0F,
    -1.0F, -1.0F, 1.0F,  1.0F, -1.0F, 1.0F,  1.0F, 1.0F, 1.0F,  -1.0F, 1.0F, 1.0F};

constexpr std::array<std::uint16_t, 36> k_cube_indices{
    4, 5, 6, 4, 6, 7, 1, 0, 3, 1, 3, 2, 0, 4, 7, 0, 7, 3,
    5, 1, 2, 5, 2, 6, 3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4};

constexpr GLint k_position_components = 3;
constexpr GLsizei k_position_stride = k_position_components * sizeof(float);

} // namespace

SkyBoxPipeline::SkyBoxPipeline(ShaderCache* shader_cache)
    : m_shader_cache(shader_cache) {
}

SkyBoxPipeline::~SkyBoxPipeline() {
  shutdown();
}

auto SkyBoxPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shader_cache == nullptr) {
    return false;
  }
  m_shader = m_shader_cache->get(QStringLiteral("sky_box"));
  if (m_shader == nullptr) {
    return false;
  }
  if (!build_cube()) {
    return false;
  }
  cache_uniforms();
  m_initialized = true;
  return true;
}

void SkyBoxPipeline::shutdown() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    release_cube();
  }
  m_vao = 0;
  m_vertex_buffer = 0;
  m_index_buffer = 0;
  m_initialized = false;
}

void SkyBoxPipeline::cache_uniforms() {
  if (m_shader == nullptr) {
    return;
  }
  m_uniforms.view_proj = m_shader->optional_uniform_handle("u_sky_box_view_proj");
  m_uniforms.blend = m_shader->optional_uniform_handle("u_sky_box_blend");
  m_uniforms.time = m_shader->optional_uniform_handle("u_sky_box_time");
}

auto SkyBoxPipeline::build_cube() -> bool {
  release_cube();

  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vertex_buffer);
  glGenBuffers(1, &m_index_buffer);
  if (m_vao == 0U || m_vertex_buffer == 0U || m_index_buffer == 0U) {
    release_cube();
    return false;
  }

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(k_cube_corners.size() * sizeof(float)),
               k_cube_corners.data(),
               GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(k_cube_indices.size() * sizeof(std::uint16_t)),
               k_cube_indices.data(),
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      0, k_position_components, GL_FLOAT, GL_FALSE, k_position_stride, nullptr);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  return true;
}

void SkyBoxPipeline::release_cube() {
  if (m_index_buffer != 0U) {
    glDeleteBuffers(1, &m_index_buffer);
    m_index_buffer = 0;
  }
  if (m_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_vertex_buffer);
    m_vertex_buffer = 0;
  }
  if (m_vao != 0U) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
}

auto SkyBoxPipeline::camera_rotation_only(const QMatrix4x4& view) -> QMatrix4x4 {
  QMatrix4x4 rotation = view;
  rotation.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
  return rotation;
}

void SkyBoxPipeline::draw(const QMatrix4x4& view,
                          const QMatrix4x4& projection,
                          float time,
                          float blend) {
  const float alpha = std::clamp(blend, 0.0F, 1.0F);
  if (!m_initialized || m_shader == nullptr || alpha <= 0.0F) {
    return;
  }

  const GLboolean depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean cull_was_enabled = glIsEnabled(GL_CULL_FACE);
  const GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, projection * camera_rotation_only(view));
  m_shader->set_uniform(m_uniforms.blend, alpha);
  m_shader->set_uniform(m_uniforms.time, time);

  glBindVertexArray(m_vao);
  glDrawElements(GL_TRIANGLES,
                 static_cast<GLsizei>(k_cube_indices.size()),
                 GL_UNSIGNED_SHORT,
                 nullptr);
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
  if (blend_was_enabled != GL_TRUE) {
    glDisable(GL_BLEND);
  }
  if (depth_was_enabled == GL_TRUE) {
    glEnable(GL_DEPTH_TEST);
  }
  if (cull_was_enabled == GL_TRUE) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
