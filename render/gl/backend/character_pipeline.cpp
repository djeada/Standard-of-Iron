#include "character_pipeline.h"
#include "../backend.h"
#include "../shader_cache.h"
#include <QDebug>
#include <qglobal.h>

namespace Render::GL::BackendPipelines {

auto CharacterPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "CharacterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basicShader = m_shaderCache->get("basic");
  m_archerShader = m_shaderCache->get("archer");
  m_knightShader = m_shaderCache->get("knight");
  m_spearmanShader = m_shaderCache->get("spearman");

  if (m_basicShader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load basic shader";
  }
  if (m_archerShader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load archer shader";
  }
  if (m_knightShader == nullptr) {
    qWarning() << "CharacterPipeline: Failed to load knight shader";
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
  m_knightShader = nullptr;
  m_spearmanShader = nullptr;
}

void CharacterPipeline::cacheUniforms() {
  cacheBasicUniforms();
  cacheArcherUniforms();
  cacheKnightUniforms();
  cacheSpearmanUniforms();
}

auto CharacterPipeline::isInitialized() const -> bool {
  return m_basicShader != nullptr && m_archerShader != nullptr &&
         m_knightShader != nullptr && m_spearmanShader != nullptr;
}

void CharacterPipeline::cacheBasicUniforms() {
  if (m_basicShader == nullptr) {
    return;
  }

  m_basicUniforms.mvp = m_basicShader->uniformHandle("u_mvp");
  m_basicUniforms.model = m_basicShader->uniformHandle("u_model");
  m_basicUniforms.texture = m_basicShader->uniformHandle("u_texture");
  m_basicUniforms.useTexture = m_basicShader->uniformHandle("u_useTexture");
  m_basicUniforms.color = m_basicShader->uniformHandle("u_color");
  m_basicUniforms.alpha = m_basicShader->uniformHandle("u_alpha");
}

void CharacterPipeline::cacheArcherUniforms() {
  if (m_archerShader == nullptr) {
    return;
  }

  m_archerUniforms.mvp = m_archerShader->uniformHandle("u_mvp");
  m_archerUniforms.model = m_archerShader->uniformHandle("u_model");
  m_archerUniforms.texture = m_archerShader->uniformHandle("u_texture");
  m_archerUniforms.useTexture = m_archerShader->uniformHandle("u_useTexture");
  m_archerUniforms.color = m_archerShader->uniformHandle("u_color");
  m_archerUniforms.alpha = m_archerShader->uniformHandle("u_alpha");
}

void CharacterPipeline::cacheKnightUniforms() {
  if (m_knightShader == nullptr) {
    return;
  }

  m_knightUniforms.mvp = m_knightShader->uniformHandle("u_mvp");
  m_knightUniforms.model = m_knightShader->uniformHandle("u_model");
  m_knightUniforms.texture = m_knightShader->uniformHandle("u_texture");
  m_knightUniforms.useTexture = m_knightShader->uniformHandle("u_useTexture");
  m_knightUniforms.color = m_knightShader->uniformHandle("u_color");
  m_knightUniforms.alpha = m_knightShader->uniformHandle("u_alpha");
}

void CharacterPipeline::cacheSpearmanUniforms() {
  if (m_spearmanShader == nullptr) {
    return;
  }

  m_spearmanUniforms.mvp = m_spearmanShader->uniformHandle("u_mvp");
  m_spearmanUniforms.model = m_spearmanShader->uniformHandle("u_model");
  m_spearmanUniforms.texture = m_spearmanShader->uniformHandle("u_texture");
  m_spearmanUniforms.useTexture =
      m_spearmanShader->uniformHandle("u_useTexture");
  m_spearmanUniforms.color = m_spearmanShader->uniformHandle("u_color");
  m_spearmanUniforms.alpha = m_spearmanShader->uniformHandle("u_alpha");
}

} // namespace Render::GL::BackendPipelines
