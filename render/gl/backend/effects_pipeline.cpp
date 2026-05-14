#include "effects_pipeline.h"

#include <QDebug>
#include <qglobal.h>

#include "../backend.h"
#include "../shader_cache.h"

namespace Render::GL::BackendPipelines {

auto EffectsPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "EffectsPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basic_shader = m_shader_cache->get("basic");
  m_basic_instanced_shader = m_shader_cache->get("basic_instanced");
  m_grid_shader = m_shader_cache->get("grid");

  if (m_basic_shader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load basic shader";
  }
  if (m_grid_shader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load grid shader";
  }

  cache_uniforms();

  return is_initialized();
}

void EffectsPipeline::shutdown() {
  m_basic_shader = nullptr;
  m_basic_instanced_shader = nullptr;
  m_grid_shader = nullptr;
}

void EffectsPipeline::cache_uniforms() {
  cache_basic_uniforms();
  cache_basic_instanced_uniforms();
  cache_grid_uniforms();
}

auto EffectsPipeline::is_initialized() const -> bool {
  return m_basic_shader != nullptr && m_grid_shader != nullptr;
}

void EffectsPipeline::cache_basic_uniforms() {
  if (m_basic_shader == nullptr) {
    return;
  }

  m_basic_uniforms.mvp = m_basic_shader->optional_uniform_handle("u_mvp");
  m_basic_uniforms.model = m_basic_shader->uniform_handle("u_model");
  m_basic_uniforms.view_proj = m_basic_shader->optional_uniform_handle("u_view_proj");
  m_basic_uniforms.texture = m_basic_shader->uniform_handle("u_texture");
  m_basic_uniforms.use_texture = m_basic_shader->uniform_handle("u_use_texture");
  m_basic_uniforms.color = m_basic_shader->uniform_handle("u_color");
  m_basic_uniforms.alpha = m_basic_shader->uniform_handle("u_alpha");
  m_basic_uniforms.instanced = m_basic_shader->optional_uniform_handle("u_instanced");
}

void EffectsPipeline::cache_basic_instanced_uniforms() {
  if (m_basic_instanced_shader == nullptr) {
    return;
  }
  m_basic_instanced_uniforms.view_proj =
      m_basic_instanced_shader->optional_uniform_handle("u_view_proj");
  m_basic_instanced_uniforms.use_texture =
      m_basic_instanced_shader->optional_uniform_handle("u_use_texture");
}

void EffectsPipeline::cache_grid_uniforms() {
  if (m_grid_shader == nullptr) {
    return;
  }

  m_grid_uniforms.mvp = m_grid_shader->optional_uniform_handle("u_mvp");
  m_grid_uniforms.model = m_grid_shader->uniform_handle("u_model");
  m_grid_uniforms.grid_color = m_grid_shader->uniform_handle("u_grid_color");
  m_grid_uniforms.line_color = m_grid_shader->uniform_handle("u_line_color");
  m_grid_uniforms.cell_size = m_grid_shader->uniform_handle("u_cell_size");
  m_grid_uniforms.thickness = m_grid_shader->uniform_handle("u_thickness");
}

} // namespace Render::GL::BackendPipelines
