#include "healing_beam_pipeline.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLContext>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "gl_error_check.h"
#include "render/gl/mesh.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader_cache.h"
#include "render/gl/state_scopes.h"
#include "static_mesh_upload.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {}

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

  return upload_static_effect_mesh(*this,
                                   m_mesh,
                                   "HealingBeamPipeline beam",
                                   vertices.data(),
                                   vertices.size(),
                                   sizeof(Vertex),
                                   k_position_normal_texcoord_layout,
                                   indices);
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
