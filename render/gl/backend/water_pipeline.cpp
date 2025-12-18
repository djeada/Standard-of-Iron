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

  cache_uniforms();

  return is_initialized();
}

void WaterPipeline::shutdown() {
  m_riverShader = nullptr;
  m_riverbankShader = nullptr;
  m_bridgeShader = nullptr;
  m_road_shader = nullptr;
}

void WaterPipeline::cache_uniforms() {
  cache_river_uniforms();
  cache_riverbank_uniforms();
  cache_bridge_uniforms();
  cache_road_uniforms();
}

auto WaterPipeline::is_initialized() const -> bool {
  return m_riverShader != nullptr && m_riverbankShader != nullptr &&
         m_bridgeShader != nullptr && m_road_shader != nullptr;
}

void WaterPipeline::cache_river_uniforms() {
  if (m_riverShader == nullptr) {
    return;
  }

  m_riverUniforms.model = m_riverShader->uniform_handle("model");
  m_riverUniforms.view = m_riverShader->uniform_handle("view");
  m_riverUniforms.projection = m_riverShader->uniform_handle("projection");
  m_riverUniforms.time = m_riverShader->uniform_handle("time");
}

void WaterPipeline::cache_riverbank_uniforms() {
  if (m_riverbankShader == nullptr) {
    return;
  }

  m_riverbankUniforms.model = m_riverbankShader->uniform_handle("model");
  m_riverbankUniforms.view = m_riverbankShader->uniform_handle("view");
  m_riverbankUniforms.projection =
      m_riverbankShader->uniform_handle("projection");
  m_riverbankUniforms.time = m_riverbankShader->uniform_handle("time");
  m_riverbankUniforms.visibility_texture =
      m_riverbankShader->uniform_handle("u_visibilityTex");
  m_riverbankUniforms.visibility_size =
      m_riverbankShader->uniform_handle("u_visibilitySize");
  m_riverbankUniforms.visibility_tile_size =
      m_riverbankShader->uniform_handle("u_visibilityTileSize");
  m_riverbankUniforms.explored_alpha =
      m_riverbankShader->uniform_handle("u_exploredAlpha");
  m_riverbankUniforms.has_visibility =
      m_riverbankShader->uniform_handle("u_hasVisibility");
  m_riverbankUniforms.segment_visibility =
      m_riverbankShader->uniform_handle("u_segmentVisibility");
}

void WaterPipeline::cache_bridge_uniforms() {
  if (m_bridgeShader == nullptr) {
    return;
  }

  m_bridgeUniforms.mvp = m_bridgeShader->uniform_handle("u_mvp");
  m_bridgeUniforms.model = m_bridgeShader->uniform_handle("u_model");
  m_bridgeUniforms.color = m_bridgeShader->uniform_handle("u_color");
  m_bridgeUniforms.light_direction =
      m_bridgeShader->uniform_handle("u_lightDirection");
}

void WaterPipeline::cache_road_uniforms() {
  if (m_road_shader == nullptr) {
    return;
  }

  m_road_uniforms.mvp = m_road_shader->uniform_handle("u_mvp");
  m_road_uniforms.model = m_road_shader->uniform_handle("u_model");
  m_road_uniforms.color = m_road_shader->uniform_handle("u_color");
  m_road_uniforms.light_direction =
      m_road_shader->uniform_handle("u_light_direction");
  m_road_uniforms.alpha = m_road_shader->uniform_handle("u_alpha");
}

} // namespace Render::GL::BackendPipelines
