#include "character_pipeline.h"
#include "../backend.h"
#include "../shader_cache.h"
#include <QDebug>
#include <qglobal.h>

namespace Render::GL::BackendPipelines {

auto CharacterPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "CharacterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basic_shader = m_shader_cache->get("basic");
  if (m_basic_shader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load basic shader";
  }

  cache_uniforms();

  return is_initialized();
}

void CharacterPipeline::shutdown() { m_basic_shader = nullptr; }

void CharacterPipeline::cache_uniforms() {
  m_uniform_cache.clear();
  cache_basic_uniforms();
}

auto CharacterPipeline::is_initialized() const -> bool {
  return m_basic_shader != nullptr;
}

void CharacterPipeline::cache_basic_uniforms() {
  if (m_basic_shader == nullptr) {
    return;
  }

  m_basic_uniforms = build_uniform_set(m_basic_shader);
  m_uniform_cache[m_basic_shader] = m_basic_uniforms;
}

auto CharacterPipeline::build_uniform_set(GL::Shader *shader) const
    -> BasicUniforms {
  BasicUniforms uniforms;
  if (shader == nullptr) {
    return uniforms;
  }
  uniforms.mvp = shader->optional_uniform_handle("u_mvp");
  uniforms.model = shader->uniform_handle("u_model");
  uniforms.texture = shader->uniform_handle("u_texture");
  uniforms.use_texture = shader->uniform_handle("u_useTexture");
  uniforms.color = shader->uniform_handle("u_color");
  uniforms.alpha = shader->uniform_handle("u_alpha");
  uniforms.material_id = shader->optional_uniform_handle("u_materialId");
  uniforms.instanced = shader->optional_uniform_handle("u_instanced");
  uniforms.view_proj = shader->optional_uniform_handle("u_viewProj");
  uniforms.light_dir = shader->optional_uniform_handle("u_lightDir");
  return uniforms;
}

auto CharacterPipeline::resolve_uniforms(GL::Shader *shader)
    -> BasicUniforms * {
  if (shader == nullptr) {
    return nullptr;
  }
  if (shader == m_last_resolved_shader) {
    return m_last_resolved_uniforms;
  }
  auto it = m_uniform_cache.find(shader);
  if (it != m_uniform_cache.end()) {
    m_last_resolved_shader = shader;
    m_last_resolved_uniforms = &it->second;
    return m_last_resolved_uniforms;
  }
  BasicUniforms uniforms = build_uniform_set(shader);
  auto [inserted, success] = m_uniform_cache.emplace(shader, uniforms);
  m_last_resolved_shader = shader;
  m_last_resolved_uniforms = &inserted->second;
  return m_last_resolved_uniforms;
}

} // namespace Render::GL::BackendPipelines
