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
  m_road_shader = m_shaderCache->get("road");

  if (m_riverShader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load river shader";
  }
  if (m_riverbankShader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load riverbank shader";
  }
  if (m_bridgeShader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load bridge shader";
  }
  if (m_road_shader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load road shader";
  }

  cacheUniforms();

  return isInitialized();
}

void WaterPipeline::shutdown() {
  m_riverShader = nullptr;
  m_riverbankShader = nullptr;
  m_bridgeShader = nullptr;
  m_road_shader = nullptr;
}

void WaterPipeline::cacheUniforms() {
  cacheRiverUniforms();
  cacheRiverbankUniforms();
  cacheBridgeUniforms();
  cache_road_uniforms();
}

auto WaterPipeline::isInitialized() const -> bool {
  return m_riverShader != nullptr && m_riverbankShader != nullptr &&
         m_bridgeShader != nullptr && m_road_shader != nullptr;
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

void WaterPipeline::cache_road_uniforms() {
  if (m_road_shader == nullptr) {
    return;
  }

  m_road_uniforms.mvp = m_road_shader->uniformHandle("u_mvp");
  m_road_uniforms.model = m_road_shader->uniformHandle("u_model");
  m_road_uniforms.color = m_road_shader->uniformHandle("u_color");
  m_road_uniforms.light_direction =
      m_road_shader->uniformHandle("u_light_direction");
  m_road_uniforms.alpha = m_road_shader->uniformHandle("u_alpha");
}

} // namespace Render::GL::BackendPipelines
