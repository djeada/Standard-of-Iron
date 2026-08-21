#include "shader_uniform_cache.h"

#include <QDebug>
#include <QString>

#include <utility>

#include "render/gl/shader_cache.h"

namespace Render::GL {

auto ShaderUniformCache::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "ShaderUniformCache::initialize: null ShaderCache";
    return false;
  }

  m_uniform_cache.clear();
  m_last_resolved_shader = nullptr;
  m_last_resolved_uniforms = nullptr;

  m_basic_shader = m_shader_cache->get(QStringLiteral("basic"));
  if (m_basic_shader == nullptr) {
    qWarning() << "ShaderUniformCache: Failed to load basic shader";
    return false;
  }

  BasicUniforms basic = build_uniform_set(m_basic_shader);
  basic.instanced_variant = m_shader_cache->get(QStringLiteral("basic_instanced"));
  m_uniform_cache[m_basic_shader] = basic;

  if (Shader* troop_shadow = m_shader_cache->get(QStringLiteral("troop_shadow"))) {
    BasicUniforms shadow = build_uniform_set(troop_shadow);
    shadow.instanced_variant =
        m_shader_cache->get(QStringLiteral("troop_shadow_instanced"));
    m_uniform_cache[troop_shadow] = shadow;
  }

  return true;
}

auto ShaderUniformCache::build_uniform_set(Shader* shader) const -> BasicUniforms {
  BasicUniforms uniforms;
  if (shader == nullptr) {
    return uniforms;
  }
  uniforms.mvp = shader->optional_uniform_handle("u_mvp");
  uniforms.model = shader->optional_uniform_handle("u_model");
  uniforms.texture = shader->optional_uniform_handle("u_texture");
  uniforms.use_texture = shader->optional_uniform_handle("u_use_texture");
  uniforms.color = shader->optional_uniform_handle("u_color");
  uniforms.alpha = shader->optional_uniform_handle("u_alpha");
  uniforms.material_id = shader->optional_uniform_handle("u_material_id");
  uniforms.material_detail = shader->optional_uniform_handle("u_material_detail");
  uniforms.has_material_detail =
      shader->optional_uniform_handle("u_has_material_detail");
  uniforms.instanced = shader->optional_uniform_handle("u_instanced");
  uniforms.view_proj = shader->optional_uniform_handle("u_view_proj");
  uniforms.light_dir = shader->optional_uniform_handle("u_light_dir");
  uniforms.ambient_strength = shader->optional_uniform_handle("u_ambient_strength");
  return uniforms;
}

auto ShaderUniformCache::resolve_uniforms(Shader* shader) -> BasicUniforms* {
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

} // namespace Render::GL
