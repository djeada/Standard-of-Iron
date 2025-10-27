#include "water_pipeline.h"
#include "../backend.h"
#include "../shader_cache.h"
#include <QDebug>
#include <qglobal.h>

namespace Render::GL::BackendPipelines {

auto WaterPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "WaterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_riverShader = m_shaderCache->get("river");
  m_riverbankShader = m_shaderCache->get("riverbank");
  m_bridgeShader = m_shaderCache->get("bridge");

  if (m_riverShader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load river shader";
  }
  if (m_riverbankShader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load riverbank shader";
  }
  if (m_bridgeShader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load bridge shader";
  }

  cacheUniforms();

  return isInitialized();
}

void WaterPipeline::shutdown() {
  m_riverShader = nullptr;
  m_riverbankShader = nullptr;
  m_bridgeShader = nullptr;
}

void WaterPipeline::cacheUniforms() {
  cacheRiverUniforms();
  cacheRiverbankUniforms();
  cacheBridgeUniforms();
}

auto WaterPipeline::isInitialized() const -> bool {
  return m_riverShader != nullptr && m_riverbankShader != nullptr &&
         m_bridgeShader != nullptr;
}

void WaterPipeline::cacheRiverUniforms() {
  if (m_riverShader == nullptr) {
    return;
  }

  m_riverUniforms.model = m_riverShader->uniformHandle("model");
  m_riverUniforms.view = m_riverShader->uniformHandle("view");
  m_riverUniforms.projection = m_riverShader->uniformHandle("projection");
  m_riverUniforms.time = m_riverShader->uniformHandle("time");
}

void WaterPipeline::cacheRiverbankUniforms() {
  if (m_riverbankShader == nullptr) {
    return;
  }

  m_riverbankUniforms.model = m_riverbankShader->uniformHandle("model");
  m_riverbankUniforms.view = m_riverbankShader->uniformHandle("view");
  m_riverbankUniforms.projection =
      m_riverbankShader->uniformHandle("projection");
  m_riverbankUniforms.time = m_riverbankShader->uniformHandle("time");
}

void WaterPipeline::cacheBridgeUniforms() {
  if (m_bridgeShader == nullptr) {
    return;
  }

  m_bridgeUniforms.mvp = m_bridgeShader->uniformHandle("u_mvp");
  m_bridgeUniforms.model = m_bridgeShader->uniformHandle("u_model");
  m_bridgeUniforms.color = m_bridgeShader->uniformHandle("u_color");
  m_bridgeUniforms.light_direction =
      m_bridgeShader->uniformHandle("u_lightDirection");
}

} // namespace Render::GL::BackendPipelines
