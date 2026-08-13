#include "mode_indicator_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>

#include "gl_error_check.h"
#include "render/gl/mesh.h"
#include "render/gl/shader_cache.h"
#include "render/gl/state_scopes.h"

namespace Render::GL::BackendPipelines {

namespace {

auto check_gl_error(const char* operation) -> bool {
  return BackendPipelines::check_gl_error("ModeIndicatorPipeline", operation);
}

} // namespace

auto ModeIndicatorPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "ModeIndicatorPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_indicator_shader = m_shader_cache->get("mode_indicator");
  if (m_indicator_shader == nullptr) {
    qWarning() << "ModeIndicatorPipeline: Failed to get mode_indicator shader";
    return false;
  }

  cache_uniforms();

  m_instanced_shader = m_shader_cache->get("mode_indicator_instanced");
  if (m_instanced_shader != nullptr) {
    cache_instanced_uniforms();
  }

  qInfo() << "ModeIndicatorPipeline initialized successfully";
  return is_initialized();
}

void ModeIndicatorPipeline::shutdown() {
  m_indicator_shader = nullptr;
  m_instanced_shader = nullptr;
}

void ModeIndicatorPipeline::cache_uniforms() {
  if (m_indicator_shader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_indicator_shader->uniform_handle("u_mvp");
  m_uniforms.mode_color = m_indicator_shader->uniform_handle("u_mode_color");
  m_uniforms.alpha = m_indicator_shader->uniform_handle("u_alpha");
  m_uniforms.time = m_indicator_shader->uniform_handle("u_time");
}

void ModeIndicatorPipeline::cache_instanced_uniforms() {
  if (m_instanced_shader == nullptr) {
    return;
  }
  m_instanced_uniforms.time = m_instanced_shader->uniform_handle("u_time");
}

auto ModeIndicatorPipeline::is_initialized() const -> bool {
  return m_indicator_shader != nullptr;
}

void ModeIndicatorPipeline::render_indicator(Mesh* mesh,
                                             const QMatrix4x4& model,
                                             const QMatrix4x4& view_proj,
                                             const QVector3D& color,
                                             float alpha,
                                             float time) {
  if (!is_initialized() || mesh == nullptr) {
    return;
  }

  DepthMaskScope const depth_mask(false);
  BlendScope const blend(true);
  DepthTestScope const depth_test(false);
  CullFaceScope const cull(false);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_indicator_shader->use();

  QMatrix4x4 mvp = view_proj * model;
  m_indicator_shader->set_uniform(m_uniforms.mvp, mvp);
  m_indicator_shader->set_uniform(m_uniforms.mode_color, color);
  m_indicator_shader->set_uniform(m_uniforms.alpha, alpha);
  m_indicator_shader->set_uniform(m_uniforms.time, time);

  mesh->draw();

  check_gl_error("render_indicator");
}

} // namespace Render::GL::BackendPipelines
