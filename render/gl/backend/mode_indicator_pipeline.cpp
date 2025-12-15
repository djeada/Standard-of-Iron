#include "mode_indicator_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../shader_cache.h"
#include "../state_scopes.h"
#include <QDebug>
#include <QOpenGLContext>

namespace Render::GL::BackendPipelines {

namespace {
void clear_gl_errors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

auto check_gl_error(const char *operation) -> bool {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "ModeIndicatorPipeline GL error in" << operation << ":"
               << err;
    return false;
  }
  return true;
}
} // namespace

auto ModeIndicatorPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "ModeIndicatorPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_indicatorShader = m_shaderCache->get("mode_indicator");
  if (m_indicatorShader == nullptr) {
    qWarning() << "ModeIndicatorPipeline: Failed to get mode_indicator shader";
    return false;
  }

  cache_uniforms();

  qInfo() << "ModeIndicatorPipeline initialized successfully";
  return is_initialized();
}

void ModeIndicatorPipeline::shutdown() { m_indicatorShader = nullptr; }

void ModeIndicatorPipeline::cache_uniforms() {
  if (m_indicatorShader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_indicatorShader->uniform_handle("u_mvp");
  m_uniforms.model = m_indicatorShader->uniform_handle("u_model");
  m_uniforms.modeColor = m_indicatorShader->uniform_handle("u_modeColor");
  m_uniforms.alpha = m_indicatorShader->uniform_handle("u_alpha");
  m_uniforms.time = m_indicatorShader->uniform_handle("u_time");
}

auto ModeIndicatorPipeline::is_initialized() const -> bool {
  return m_indicatorShader != nullptr;
}

void ModeIndicatorPipeline::render_indicator(Mesh *mesh,
                                              const QMatrix4x4 &model,
                                              const QMatrix4x4 &view_proj,
                                              const QVector3D &color,
                                              float alpha, float time) {
  if (!is_initialized() || mesh == nullptr) {
    return;
  }

  // Use state scopes for automatic GL state management
  DepthMaskScope const depth_mask(false);
  BlendScope const blend(true);
  
  glEnable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for glow effect

  m_indicatorShader->use();

  QMatrix4x4 mvp = view_proj * model;
  m_indicatorShader->set_uniform(m_uniforms.mvp, mvp);
  m_indicatorShader->set_uniform(m_uniforms.model, model);
  m_indicatorShader->set_uniform(m_uniforms.modeColor, color);
  m_indicatorShader->set_uniform(m_uniforms.alpha, alpha);
  m_indicatorShader->set_uniform(m_uniforms.time, time);

  mesh->draw();

  check_gl_error("render_indicator");
}

} // namespace Render::GL::BackendPipelines
