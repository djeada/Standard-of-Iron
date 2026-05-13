#include "character_pipeline.h"
#include "../backend.h"
#include "../shader_cache.h"
#include <QDebug>
#include <QStringList>
#include <qglobal.h>
#include <utility>

namespace Render::GL::BackendPipelines {

namespace {

auto resolve_unit_shader(GL::ShaderCache *shader_cache, GL::Shader *fallback,
                         const QString &name) -> GL::Shader * {
  if (shader_cache == nullptr) {
    return fallback;
  }
  if (GL::Shader *shader = shader_cache->get(name)) {
    return shader;
  }
  return fallback;
}

} // namespace

auto CharacterPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "CharacterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basic_shader = m_shader_cache->get("basic");
  m_archer_shader = resolve_unit_shader(m_shader_cache, m_basic_shader,
                                        QStringLiteral("archer"));
  m_swordsman_shader = resolve_unit_shader(m_shader_cache, m_basic_shader,
                                           QStringLiteral("swordsman"));
  m_spearman_shader = resolve_unit_shader(m_shader_cache, m_basic_shader,
                                          QStringLiteral("spearman"));

  if (m_basic_shader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load basic shader";
  }

  cache_uniforms();

  return is_initialized();
}

void CharacterPipeline::shutdown() {
  m_basic_shader = nullptr;
  m_archer_shader = nullptr;
  m_swordsman_shader = nullptr;
  m_spearman_shader = nullptr;
}

void CharacterPipeline::cache_uniforms() {
  m_uniform_cache.clear();
  cache_basic_uniforms();
  cache_archer_uniforms();
  cache_knight_uniforms();
  cache_spearman_uniforms();
}

auto CharacterPipeline::is_initialized() const -> bool {
  return m_basic_shader != nullptr;
}

void CharacterPipeline::cache_basic_uniforms() {
  if (m_basic_shader == nullptr) {
    return;
  }

  m_basic_uniforms = build_uniform_set(m_basic_shader);
  if (m_shader_cache != nullptr) {
    m_basic_uniforms.instanced_variant =
        m_shader_cache->get(QStringLiteral("basic_instanced"));
  }
  m_uniform_cache[m_basic_shader] = m_basic_uniforms;
}

void CharacterPipeline::cache_archer_uniforms() {
  if (m_archer_shader == nullptr) {
    return;
  }

  m_archer_uniforms = build_uniform_set(m_archer_shader);
  m_uniform_cache[m_archer_shader] = m_archer_uniforms;
  cache_nation_variants(QStringLiteral("archer"));
}

void CharacterPipeline::cache_knight_uniforms() {
  if (m_swordsman_shader == nullptr) {
    return;
  }

  m_swordsman_uniforms = build_uniform_set(m_swordsman_shader);
  m_uniform_cache[m_swordsman_shader] = m_swordsman_uniforms;
  cache_nation_variants(QStringLiteral("swordsman"));
}

void CharacterPipeline::cache_spearman_uniforms() {
  if (m_spearman_shader == nullptr) {
    return;
  }

  m_spearman_uniforms = build_uniform_set(m_spearman_shader);
  m_uniform_cache[m_spearman_shader] = m_spearman_uniforms;
  cache_nation_variants(QStringLiteral("spearman"));
}

auto CharacterPipeline::build_uniform_set(GL::Shader *shader) const
    -> BasicUniforms {
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
  uniforms.instanced = shader->optional_uniform_handle("u_instanced");
  uniforms.view_proj = shader->optional_uniform_handle("u_view_proj");
  uniforms.light_dir = shader->optional_uniform_handle("u_light_dir");
  uniforms.ambient_strength =
      shader->optional_uniform_handle("u_ambient_strength");
  return uniforms;
}

void CharacterPipeline::cache_nation_variants(const QString &base_key) {
  if (m_shader_cache == nullptr) {
    return;
  }
  static const QStringList nations{QStringLiteral("roman_republic"),
                                   QStringLiteral("carthage")};
  for (const QString &nation : nations) {
    const QString shader_name = base_key + QStringLiteral("_") + nation;
    if (GL::Shader *variant = m_shader_cache->get(shader_name)) {
      m_uniform_cache.emplace(variant, build_uniform_set(variant));
    }
  }
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
