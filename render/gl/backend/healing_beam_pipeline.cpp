#include "healing_beam_pipeline.h"
#include "../../../game/systems/healing_beam.h"
#include "../../../game/systems/healing_beam_system.h"
#include "../backend.h"
#include "../camera.h"
#include "../mesh.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include <QDebug>
#include <QMatrix4x4>
#include <cmath>
#include <numbers>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

void clear_gl_errors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

auto check_gl_error(const char *operation) -> bool {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "HealingBeamPipeline GL error in" << operation << ":" << err;
    return false;
  }
  return true;
}
} // namespace

auto HealingBeamPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "HealingBeamPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_beamShader = m_shaderCache->get("healing_beam");
  if (m_beamShader == nullptr) {
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
  shutdown_geometry();
  m_beamShader = nullptr;
}

void HealingBeamPipeline::shutdown_geometry() {
  initializeOpenGLFunctions();
  clear_gl_errors();

  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
  if (m_vertexBuffer != 0) {
    glDeleteBuffers(1, &m_vertexBuffer);
    m_vertexBuffer = 0;
  }
  if (m_indexBuffer != 0) {
    glDeleteBuffers(1, &m_indexBuffer);
    m_indexBuffer = 0;
  }
  m_indexCount = 0;
}

void HealingBeamPipeline::cache_uniforms() {
  if (m_beamShader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_beamShader->uniform_handle("u_mvp");
  m_uniforms.time = m_beamShader->uniform_handle("u_time");
  m_uniforms.progress = m_beamShader->uniform_handle("u_progress");
  m_uniforms.startPos = m_beamShader->uniform_handle("u_startPos");
  m_uniforms.endPos = m_beamShader->uniform_handle("u_endPos");
  m_uniforms.beamWidth = m_beamShader->uniform_handle("u_beamWidth");
  m_uniforms.healColor = m_beamShader->uniform_handle("u_healColor");
  m_uniforms.alpha = m_beamShader->uniform_handle("u_alpha");
}

auto HealingBeamPipeline::is_initialized() const -> bool {
  return m_beamShader != nullptr && m_vao != 0 && m_indexCount > 0;
}

auto HealingBeamPipeline::create_beam_geometry() -> bool {
  initializeOpenGLFunctions();
  shutdown_geometry();
  clear_gl_errors();

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int segments_along = 24;
  constexpr int segments_around = 8;
  constexpr float pi = std::numbers::pi_v<float>;

  vertices.reserve(
      static_cast<size_t>((segments_along + 1) * (segments_around + 1)));
  indices.reserve(static_cast<size_t>(segments_along * segments_around * 6));

  for (int i = 0; i <= segments_along; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(segments_along);

    for (int j = 0; j <= segments_around; ++j) {
      float angle = static_cast<float>(j) /
                    static_cast<float>(segments_around) * 2.0F * pi;

      float x = std::cos(angle);
      float y = std::sin(angle);

      Vertex v;
      v.position = {x, y, t};
      v.normal = {x, y, 0.0F};
      v.tex_coord = {
          static_cast<float>(j) / static_cast<float>(segments_around), t};
      vertices.push_back(v);
    }
  }

  for (int i = 0; i < segments_along; ++i) {
    for (int j = 0; j < segments_around; ++j) {
      unsigned int curr =
          static_cast<unsigned int>(i * (segments_around + 1) + j);
      unsigned int next = curr + static_cast<unsigned int>(segments_around + 1);

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
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

  glGenBuffers(1, &m_vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("vertex buffer")) {
    shutdown_geometry();
    return false;
  }

  glGenBuffers(1, &m_indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("index buffer")) {
    shutdown_geometry();
    return false;
  }

  m_indexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(VertexAttrib::Position);
  glVertexAttribPointer(VertexAttrib::Position, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));

  glEnableVertexAttribArray(VertexAttrib::Normal);
  glVertexAttribPointer(VertexAttrib::Normal, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));

  glEnableVertexAttribArray(VertexAttrib::TexCoord);
  glVertexAttribPointer(VertexAttrib::TexCoord, ComponentCount::Vec2, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glBindVertexArray(0);

  if (!check_gl_error("vertex attributes")) {
    shutdown_geometry();
    return false;
  }

  return true;
}

void HealingBeamPipeline::render(
    const Game::Systems::HealingBeamSystem *beam_system, const Camera &cam,
    float animation_time) {
  if (!is_initialized()) {
    return;
  }
  if (beam_system == nullptr || beam_system->get_beam_count() == 0) {
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean blendEnabled = glIsEnabled(GL_BLEND);
  GLboolean depthMaskEnabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);

  GLint prevBlendSrc = 0;
  GLint prevBlendDst = 0;
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrc);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDst);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  m_beamShader->use();
  glBindVertexArray(m_vao);

  for (const auto &beam : beam_system->get_beams()) {
    if (beam && beam->is_active()) {
      render_beam(*beam, cam, animation_time);
    }
  }

  glBindVertexArray(0);

  glDepthMask(depthMaskEnabled);
  glBlendFunc(static_cast<GLenum>(prevBlendSrc),
              static_cast<GLenum>(prevBlendDst));
  if (!blendEnabled) {
    glDisable(GL_BLEND);
  }
  if (depthTestEnabled) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  if (cullEnabled) {
    glEnable(GL_CULL_FACE);
  }
}

void HealingBeamPipeline::render_beam(const Game::Systems::HealingBeam &beam,
                                      const Camera &cam, float animation_time) {
  QMatrix4x4 mvp = cam.get_projection_matrix() * cam.get_view_matrix();

  float progress = std::clamp(beam.get_progress(), 0.0F, 1.0F);
  float alpha = std::clamp(beam.get_intensity(), 0.0F, 1.0F);

  if (alpha < 0.01F) {
    return;
  }

  m_beamShader->set_uniform(m_uniforms.mvp, mvp);
  m_beamShader->set_uniform(m_uniforms.time, animation_time);
  m_beamShader->set_uniform(m_uniforms.progress, progress);
  m_beamShader->set_uniform(m_uniforms.startPos, beam.get_start());
  m_beamShader->set_uniform(m_uniforms.endPos, beam.get_end());
  m_beamShader->set_uniform(m_uniforms.beamWidth, beam.get_beam_width());
  m_beamShader->set_uniform(m_uniforms.healColor, beam.get_color());
  m_beamShader->set_uniform(m_uniforms.alpha, alpha);

  glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
}

void HealingBeamPipeline::render_single_beam(const QVector3D &start,
                                             const QVector3D &end,
                                             const QVector3D &color,
                                             float progress, float beam_width,
                                             float intensity, float time,
                                             const QMatrix4x4 &view_proj) {
  if (!is_initialized()) {
    return;
  }
  if (intensity < 0.01F) {
    return;
  }

  initializeOpenGLFunctions();

  GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depthMaskEnabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  m_beamShader->use();
  glBindVertexArray(m_vao);

  m_beamShader->set_uniform(m_uniforms.mvp, view_proj);
  m_beamShader->set_uniform(m_uniforms.time, time);
  m_beamShader->set_uniform(m_uniforms.progress,
                            std::clamp(progress, 0.0F, 1.0F));
  m_beamShader->set_uniform(m_uniforms.startPos, start);
  m_beamShader->set_uniform(m_uniforms.endPos, end);
  m_beamShader->set_uniform(m_uniforms.beamWidth, beam_width);
  m_beamShader->set_uniform(m_uniforms.healColor, color);
  m_beamShader->set_uniform(m_uniforms.alpha,
                            std::clamp(intensity, 0.0F, 1.0F));

  glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);

  glBindVertexArray(0);

  glDepthMask(depthMaskEnabled);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (cullEnabled) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
