#include "effects_pipeline.h"
#include "../backend.h"
#include "../shader_cache.h"
#include <QDebug>
#include <qglobal.h>

namespace Render::GL::BackendPipelines {

auto EffectsPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "EffectsPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basicShader = m_shaderCache->get("basic");
  m_gridShader = m_shaderCache->get("grid");

  if (m_basicShader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load basic shader";
  }
  if (m_gridShader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load grid shader";
  }

  cache_uniforms();

  return is_initialized();
}

void EffectsPipeline::shutdown() {
  m_basicShader = nullptr;
  m_gridShader = nullptr;
}

void EffectsPipeline::cache_uniforms() {
  cache_basic_uniforms();
  cache_grid_uniforms();
}

auto EffectsPipeline::is_initialized() const -> bool {
  return m_basicShader != nullptr && m_gridShader != nullptr;
}

void EffectsPipeline::cache_basic_uniforms() {
  if (m_basicShader == nullptr) {
    return;
  }

  m_basicUniforms.mvp = m_basicShader->uniform_handle("u_mvp");
  m_basicUniforms.model = m_basicShader->uniform_handle("u_model");
  m_basicUniforms.texture = m_basicShader->uniform_handle("u_texture");
  m_basicUniforms.useTexture = m_basicShader->uniform_handle("u_useTexture");
  m_basicUniforms.color = m_basicShader->uniform_handle("u_color");
  m_basicUniforms.alpha = m_basicShader->uniform_handle("u_alpha");
}

void EffectsPipeline::cache_grid_uniforms() {
  if (m_gridShader == nullptr) {
    return;
  }

  m_gridUniforms.mvp = m_gridShader->uniform_handle("u_mvp");
  m_gridUniforms.model = m_gridShader->uniform_handle("u_model");
  m_gridUniforms.gridColor = m_gridShader->uniform_handle("u_gridColor");
  m_gridUniforms.lineColor = m_gridShader->uniform_handle("u_lineColor");
  m_gridUniforms.cellSize = m_gridShader->uniform_handle("u_cellSize");
  m_gridUniforms.thickness = m_gridShader->uniform_handle("u_thickness");
}

} 
