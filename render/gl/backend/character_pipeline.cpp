#include "character_pipeline.h"
#include "../backend.h"
#include "../shader_cache.h"
#include <QDebug>
#include <QStringList>
#include <qglobal.h>
#include <utility>

namespace Render::GL::BackendPipelines {

auto CharacterPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "CharacterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basicShader = m_shaderCache->get("basic");
  m_archerShader = m_shaderCache->get("archer");
  m_swordsmanShader = m_shaderCache->get("swordsman");
  m_spearmanShader = m_shaderCache->get("spearman");

  if (m_basicShader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load basic shader";
  }
  if (m_archerShader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load archer shader";
  }
  if (m_swordsmanShader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load swordsman shader";
  }
  if (m_spearmanShader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load spearman shader";
  }

  cache_uniforms();

  return is_initialized();
}

void CharacterPipeline::shutdown() {
  m_basicShader = nullptr;
  m_archerShader = nullptr;
  m_swordsmanShader = nullptr;
  m_spearmanShader = nullptr;
}

void CharacterPipeline::cache_uniforms() {
  m_uniformCache.clear();
  cache_basic_uniforms();
  cache_archer_uniforms();
  cache_knight_uniforms();
  cache_spearman_uniforms();
}

auto CharacterPipeline::is_initialized() const -> bool {
  return m_basicShader != nullptr && m_archerShader != nullptr &&
         m_swordsmanShader != nullptr && m_spearmanShader != nullptr;
}

void CharacterPipeline::cache_basic_uniforms() {
  if (m_basicShader == nullptr) {
    return;
  }

  m_basicUniforms = buildUniformSet(m_basicShader);
  m_uniformCache[m_basicShader] = m_basicUniforms;
}

void CharacterPipeline::cache_archer_uniforms() {
  if (m_archerShader == nullptr) {
    return;
  }

  m_archerUniforms = buildUniformSet(m_archerShader);
  m_uniformCache[m_archerShader] = m_archerUniforms;
  cache_nation_variants(QStringLiteral("archer"));
}

void CharacterPipeline::cache_knight_uniforms() {
  if (m_swordsmanShader == nullptr) {
    return;
  }

  m_swordsmanUniforms = buildUniformSet(m_swordsmanShader);
  m_uniformCache[m_swordsmanShader] = m_swordsmanUniforms;
  cache_nation_variants(QStringLiteral("swordsman"));
}

void CharacterPipeline::cache_spearman_uniforms() {
  if (m_spearmanShader == nullptr) {
    return;
  }

  m_spearmanUniforms = buildUniformSet(m_spearmanShader);
  m_uniformCache[m_spearmanShader] = m_spearmanUniforms;
  cache_nation_variants(QStringLiteral("spearman"));
}

auto CharacterPipeline::buildUniformSet(GL::Shader *shader) const
    -> BasicUniforms {
  BasicUniforms uniforms;
  if (shader == nullptr) {
    return uniforms;
  }
  uniforms.mvp = shader->uniform_handle("u_mvp");
  uniforms.model = shader->uniform_handle("u_model");
  uniforms.texture = shader->uniform_handle("u_texture");
  uniforms.useTexture = shader->uniform_handle("u_useTexture");
  uniforms.color = shader->uniform_handle("u_color");
  uniforms.alpha = shader->uniform_handle("u_alpha");
  uniforms.materialId = shader->optional_uniform_handle("u_materialId");
  return uniforms;
}

void CharacterPipeline::cache_nation_variants(const QString &baseKey) {
  if (m_shaderCache == nullptr) {
    return;
  }
  static const QStringList nations{QStringLiteral("roman_republic"),
                                   QStringLiteral("carthage")};
  for (const QString &nation : nations) {
    const QString shaderName = baseKey + QStringLiteral("_") + nation;
    if (GL::Shader *variant = m_shaderCache->get(shaderName)) {
      m_uniformCache.emplace(variant, buildUniformSet(variant));
    }
  }
}

auto CharacterPipeline::resolveUniforms(GL::Shader *shader) -> BasicUniforms * {
  if (shader == nullptr) {
    return nullptr;
  }
  auto it = m_uniformCache.find(shader);
  if (it != m_uniformCache.end()) {
    return &it->second;
  }
  BasicUniforms uniforms = buildUniformSet(shader);
  auto [inserted, success] = m_uniformCache.emplace(shader, uniforms);
  return &inserted->second;
}

} 
