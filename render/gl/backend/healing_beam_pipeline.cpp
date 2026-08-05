#include "healing_beam_pipeline.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLContext>

#include <cmath>
#include <numbers>

#include "../../../game/systems/healing_beam.h"
#include "../../../game/systems/healing_beam_system.h"
#include "../backend.h"
#include "../mesh.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include "../state_scopes.h"
#include "gl_error_check.h"
#include "scene/camera.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

auto check_gl_error(const char* operation) -> bool {
  return BackendPipelines::check_gl_error("HealingBeamPipeline", operation);
}

} // namespace

auto HealingBeamPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "HealingBeamPipeline::initialize: null ShaderCache";
    return false;
  }

  clear_gl_errors();

  m_beam_shader = m_shader_cache->get("healing_beam");
  if (m_beam_shader == nullptr) {
    qWarning() << "HealingBeamPipeline: Failed to get healing_beam shader";
    return false;
  }

  cache_uniforms();

  if (!create_beam_geometry()) {
    qWarning() << "HealingBeamPipeline: Failed to create beam geometry";
    return false;
  }

  qInfo() << "HealingBeamPipeline initialized successfully";
  return is_initialized();
}

void HealingBeamPipeline::shutdown() {
  release_geometry();
  m_beam_shader = nullptr;
}

void HealingBeamPipeline::release_geometry() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    clear_gl_errors();
  }
  release_mesh_buffers(*this, m_mesh);
}

void HealingBeamPipeline::cache_uniforms() {
  if (m_beam_shader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_beam_shader->uniform_handle("u_mvp");
  m_uniforms.time = m_beam_shader->uniform_handle("u_time");
  m_uniforms.progress = m_beam_shader->uniform_handle("u_progress");
  m_uniforms.start_pos = m_beam_shader->uniform_handle("u_start_pos");
  m_uniforms.end_pos = m_beam_shader->uniform_handle("u_end_pos");
  m_uniforms.beam_width = m_beam_shader->uniform_handle("u_beam_width");
  m_uniforms.heal_color = m_beam_shader->uniform_handle("u_heal_color");
  m_uniforms.alpha = m_beam_shader->uniform_handle("u_alpha");
}

auto HealingBeamPipeline::is_initialized() const -> bool {
  return m_beam_shader != nullptr && m_mesh.vao != 0 && m_mesh.index_count > 0;
}

auto HealingBeamPipeline::create_beam_geometry() -> bool {
  initializeOpenGLFunctions();
  release_geometry();
  clear_gl_errors();

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int segments_along = 24;
  constexpr int segments_around = 8;
  constexpr float pi = std::numbers::pi_v<float>;

  vertices.reserve(static_cast<size_t>((segments_along + 1) * (segments_around + 1)));
  indices.reserve(static_cast<size_t>(segments_along * segments_around * 6));

  for (int i = 0; i <= segments_along; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(segments_along);

    for (int j = 0; j <= segments_around; ++j) {
      float angle =
          static_cast<float>(j) / static_cast<float>(segments_around) * 2.0F * pi;

      float x = std::cos(angle);
      float y = std::sin(angle);

      Vertex v;
      v.position = {x, y, t};
      v.normal = {x, y, 0.0F};
      v.tex_coord = {static_cast<float>(j) / static_cast<float>(segments_around), t};
      vertices.push_back(v);
    }
  }

  for (int i = 0; i < segments_along; ++i) {
    for (int j = 0; j < segments_around; ++j) {
      unsigned int curr = static_cast<unsigned int>(i * (segments_around + 1) + j);
      unsigned int next = curr + static_cast<unsigned int>(segments_around + 1);

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
  }

  glGenVertexArrays(1, &m_mesh.vao);
  if (!check_gl_error("glGenVertexArrays") || m_mesh.vao == 0) {
    return false;
  }

  glBindVertexArray(m_mesh.vao);
  if (!check_gl_error("glBindVertexArray")) {
    glDeleteVertexArrays(1, &m_mesh.vao);
    m_mesh.vao = 0;
    return false;
  }

  glGenBuffers(1, &m_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(),
               GL_STATIC_DRAW);
  if (!check_gl_error("vertex buffer")) {
    release_geometry();
    return false;
  }

  glGenBuffers(1, &m_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(),
               GL_STATIC_DRAW);
  if (!check_gl_error("index buffer")) {
    release_geometry();
    return false;
  }

  m_mesh.index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(VertexAttrib::position);
  glVertexAttribPointer(VertexAttrib::position,
                        ComponentCount::vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, position)));

  glEnableVertexAttribArray(VertexAttrib::normal);
  glVertexAttribPointer(VertexAttrib::normal,
                        ComponentCount::vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, normal)));

  glEnableVertexAttribArray(VertexAttrib::tex_coord);
  glVertexAttribPointer(VertexAttrib::tex_coord,
                        ComponentCount::vec2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, tex_coord)));

  glBindVertexArray(0);

  if (!check_gl_error("vertex attributes")) {
    release_geometry();
    return false;
  }

  return true;
}

void HealingBeamPipeline::render(const Game::Systems::HealingBeamSystem* beam_system,
                                 const Camera& cam,
                                 float animation_time) {
  if (!is_initialized()) {
    return;
  }
  if (beam_system == nullptr || beam_system->get_beam_count() == 0) {
    return;
  }

  clear_gl_errors();

  CullFaceScope const cull(false);
  DepthTestScope const depth_test(true);
  DepthMaskScope const depth_mask(false);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  m_beam_shader->use();
  glBindVertexArray(m_mesh.vao);

  for (const auto& beam : beam_system->get_beams()) {
    if (beam && beam->is_active()) {
      render_beam(*beam, cam, animation_time);
    }
  }

  glBindVertexArray(0);
}

void HealingBeamPipeline::render_beam(const Game::Systems::HealingBeam& beam,
                                      const Camera& cam,
                                      float animation_time) {
  QMatrix4x4 mvp = cam.get_view_projection_matrix();

  float progress = std::clamp(beam.get_progress(), 0.0F, 1.0F);
  float alpha = std::clamp(beam.get_intensity(), 0.0F, 1.0F);

  if (alpha < 0.01F) {
    return;
  }

  m_beam_shader->set_uniform(m_uniforms.mvp, mvp);
  m_beam_shader->set_uniform(m_uniforms.time, animation_time);
  m_beam_shader->set_uniform(m_uniforms.progress, progress);
  m_beam_shader->set_uniform(m_uniforms.start_pos, beam.get_start());
  m_beam_shader->set_uniform(m_uniforms.end_pos, beam.get_end());
  m_beam_shader->set_uniform(m_uniforms.beam_width, beam.get_beam_width());
  m_beam_shader->set_uniform(m_uniforms.heal_color, beam.get_color());
  m_beam_shader->set_uniform(m_uniforms.alpha, alpha);

  glDrawElements(GL_TRIANGLES, m_mesh.index_count, GL_UNSIGNED_INT, nullptr);
}

void HealingBeamPipeline::render_single_beam(const QVector3D& start,
                                             const QVector3D& end,
                                             const QVector3D& color,
                                             float progress,
                                             float beam_width,
                                             float intensity,
                                             float time,
                                             const QMatrix4x4& view_proj) {
  if (!is_initialized()) {
    return;
  }
  if (intensity < 0.01F) {
    return;
  }

  CullFaceScope const cull(false);
  DepthTestScope const depth_test(true);
  DepthMaskScope const depth_mask(false);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  m_beam_shader->use();
  glBindVertexArray(m_mesh.vao);

  m_beam_shader->set_uniform(m_uniforms.mvp, view_proj);
  m_beam_shader->set_uniform(m_uniforms.time, time);
  m_beam_shader->set_uniform(m_uniforms.progress, std::clamp(progress, 0.0F, 1.0F));
  m_beam_shader->set_uniform(m_uniforms.start_pos, start);
  m_beam_shader->set_uniform(m_uniforms.end_pos, end);
  m_beam_shader->set_uniform(m_uniforms.beam_width, beam_width);
  m_beam_shader->set_uniform(m_uniforms.heal_color, color);
  m_beam_shader->set_uniform(m_uniforms.alpha, std::clamp(intensity, 0.0F, 1.0F));

  glDrawElements(GL_TRIANGLES, m_mesh.index_count, GL_UNSIGNED_INT, nullptr);

  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
