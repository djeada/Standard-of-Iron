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

  cacheUniforms();

  return isInitialized();
}

void CharacterPipeline::shutdown() {
  m_basicShader = nullptr;
  m_archerShader = nullptr;
  m_swordsmanShader = nullptr;
  m_spearmanShader = nullptr;
}

void CharacterPipeline::cacheUniforms() {
  m_uniformCache.clear();
  cacheBasicUniforms();
  cacheArcherUniforms();
  cacheKnightUniforms();
  cacheSpearmanUniforms();
}

auto CharacterPipeline::isInitialized() const -> bool {
  return m_basicShader != nullptr && m_archerShader != nullptr &&
         m_swordsmanShader != nullptr && m_spearmanShader != nullptr;
}

void CharacterPipeline::cacheBasicUniforms() {
  if (m_basicShader == nullptr) {
    return;
  }

  m_basicUniforms = buildUniformSet(m_basicShader);
  m_uniformCache[m_basicShader] = m_basicUniforms;
}

void CharacterPipeline::cacheArcherUniforms() {
  if (m_archerShader == nullptr) {
    return;
  }

  m_archerUniforms = buildUniformSet(m_archerShader);
  m_uniformCache[m_archerShader] = m_archerUniforms;
  cacheNationVariants(QStringLiteral("archer"));
}

void CharacterPipeline::cacheKnightUniforms() {
  if (m_swordsmanShader == nullptr) {
    return;
  }

  m_swordsmanUniforms = buildUniformSet(m_swordsmanShader);
  m_uniformCache[m_swordsmanShader] = m_swordsmanUniforms;
  cacheNationVariants(QStringLiteral("swordsman"));
}

void CharacterPipeline::cacheSpearmanUniforms() {
  if (m_spearmanShader == nullptr) {
    return;
  }

  m_spearmanUniforms = buildUniformSet(m_spearmanShader);
  m_uniformCache[m_spearmanShader] = m_spearmanUniforms;
  cacheNationVariants(QStringLiteral("spearman"));
}

auto CharacterPipeline::buildUniformSet(GL::Shader *shader) const
    -> BasicUniforms {
  BasicUniforms uniforms;
  if (shader == nullptr) {
    return uniforms;
  }
  uniforms.mvp = shader->uniformHandle("u_mvp");
  uniforms.model = shader->uniformHandle("u_model");
  uniforms.texture = shader->uniformHandle("u_texture");
  uniforms.useTexture = shader->uniformHandle("u_useTexture");
  uniforms.color = shader->uniformHandle("u_color");
  uniforms.alpha = shader->uniformHandle("u_alpha");
  return uniforms;
}

void CharacterPipeline::cacheNationVariants(const QString &baseKey) {
  if (m_shaderCache == nullptr) {
    return;
  }
  static const QStringList nations{QStringLiteral("kingdom_of_iron"),
                                   QStringLiteral("roman_republic"),
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

} // namespace Render::GL::BackendPipelines
