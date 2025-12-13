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

auto HealingBeamPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "HealingBeamPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();

  m_beamShader = m_shaderCache->get("healing_beam");
  if (m_beamShader == nullptr) {
    qWarning() << "HealingBeamPipeline: Failed to get healing_beam shader";
    return false;
  }

  cache_uniforms();
  create_beam_geometry();

  if (m_vao == 0) {
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
  m_uniforms.model = m_beamShader->optional_uniform_handle("u_model");
  m_uniforms.time = m_beamShader->uniform_handle("u_time");
  m_uniforms.progress = m_beamShader->uniform_handle("u_progress");
  m_uniforms.startPos = m_beamShader->uniform_handle("u_startPos");
  m_uniforms.endPos = m_beamShader->uniform_handle("u_endPos");
  m_uniforms.beamWidth = m_beamShader->uniform_handle("u_beamWidth");
  m_uniforms.healColor = m_beamShader->uniform_handle("u_healColor");
  m_uniforms.alpha = m_beamShader->uniform_handle("u_alpha");

  qDebug() << "HealingBeamPipeline uniforms cached:"
           << "mvp=" << m_uniforms.mvp << "time=" << m_uniforms.time
           << "progress=" << m_uniforms.progress;
}

auto HealingBeamPipeline::is_initialized() const -> bool {
  return m_beamShader != nullptr && m_vao != 0;
}

void HealingBeamPipeline::create_beam_geometry() {
  initializeOpenGLFunctions();
  shutdown_geometry();

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  // Create a tube mesh along Z axis (0 to 1)
  // The vertex shader will deform this along the beam path
  constexpr int segments_along = 32;  // Along beam length
  constexpr int segments_around = 12; // Around beam circumference
  constexpr float pi = std::numbers::pi_v<float>;

  for (int i = 0; i <= segments_along; ++i) {
    float t = static_cast<float>(i) / segments_along;

    for (int j = 0; j <= segments_around; ++j) {
      float angle = static_cast<float>(j) / segments_around * 2.0F * pi;

      float x = std::cos(angle);
      float y = std::sin(angle);

      // Position: x,y define cross-section, z is position along beam (t)
      Vertex v;
      v.position = {x, y, t};
      v.normal = {x, y, 0.0F};
      v.tex_coord = {static_cast<float>(j) / segments_around, t};
      vertices.push_back(v);
    }
  }

  // Generate indices for triangles
  for (int i = 0; i < segments_along; ++i) {
    for (int j = 0; j < segments_around; ++j) {
      int curr = i * (segments_around + 1) + j;
      int next = curr + segments_around + 1;

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
  }

  // Create VAO
  glGenVertexArrays(1, &m_vao);
  glBindVertexArray(m_vao);

  // Create vertex buffer
  glGenBuffers(1, &m_vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  // Create index buffer
  glGenBuffers(1, &m_indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_indexCount = static_cast<GLsizei>(indices.size());

  // Set up vertex attributes (position, normal, texcoord)
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

  qDebug() << "HealingBeamPipeline geometry created: vertices="
           << vertices.size() << "indices=" << indices.size() << "vao=" << m_vao;
}

void HealingBeamPipeline::render(
    const Game::Systems::HealingBeamSystem *beam_system, const Camera &cam,
    float animation_time) {
  if (!is_initialized() || beam_system == nullptr ||
      beam_system->get_beam_count() == 0) {
    return;
  }

  // Save current GL state
  GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean blendEnabled = glIsEnabled(GL_BLEND);
  GLboolean depthMaskEnabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);

  // Set up state for glow rendering
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for glow
  glDepthMask(GL_FALSE);

  m_beamShader->use();
  
  // Clear any pre-existing errors from previous operations
  while (glGetError() != GL_NO_ERROR) {}
  
  glBindVertexArray(m_vao);

  GLenum vaoErr = glGetError();
  if (vaoErr != GL_NO_ERROR) {
    qWarning() << "HealingBeamPipeline glBindVertexArray error:" << vaoErr
               << "vao=" << m_vao;
  }

  for (const auto &beam : beam_system->get_beams()) {
    if (beam && beam->is_active()) {
      render_beam(*beam, cam, animation_time);
    }
  }

  glBindVertexArray(0);

  // Restore GL state
  glDepthMask(depthMaskEnabled);
  if (!blendEnabled) {
    glDisable(GL_BLEND);
  }
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (depthTestEnabled) {
    glEnable(GL_DEPTH_TEST);
  }
  if (cullEnabled) {
    glEnable(GL_CULL_FACE);
  }
}

void HealingBeamPipeline::render_beam(const Game::Systems::HealingBeam &beam,
                                      const Camera &cam, float animation_time) {
  QMatrix4x4 model;
  model.setToIdentity();

  QMatrix4x4 mvp =
      cam.get_projection_matrix() * cam.get_view_matrix() * model;

  m_beamShader->set_uniform(m_uniforms.mvp, mvp);
  if (m_uniforms.model != Shader::InvalidUniform) {
    m_beamShader->set_uniform(m_uniforms.model, model);
  }
  m_beamShader->set_uniform(m_uniforms.time, animation_time);
  m_beamShader->set_uniform(m_uniforms.progress,
                            std::min(beam.get_progress(), 1.0F));
  m_beamShader->set_uniform(m_uniforms.startPos, beam.get_start());
  m_beamShader->set_uniform(m_uniforms.endPos, beam.get_end());
  m_beamShader->set_uniform(m_uniforms.beamWidth, beam.get_beam_width());
  m_beamShader->set_uniform(m_uniforms.healColor, beam.get_color());
  m_beamShader->set_uniform(m_uniforms.alpha, beam.get_intensity());

  glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "HealingBeamPipeline glDrawElements error:" << err;
  }
}

} // namespace Render::GL::BackendPipelines
