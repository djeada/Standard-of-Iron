#include "water_pipeline.h"

#include <QDebug>
#include <qglobal.h>

#include "../backend.h"
#include "../shader_cache.h"

namespace Render::GL::BackendPipelines {

auto WaterPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "WaterPipeline::initialize: null ShaderCache";
    return false;
  }

  m_river_shader = m_shader_cache->get("river");
  m_riverbank_shader = m_shader_cache->get("riverbank");
  m_bridge_shader = m_shader_cache->get("bridge");
  m_road_shader = m_shader_cache->get("road");

  if (m_river_shader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load river shader";
  }
  if (m_riverbank_shader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load riverbank shader";
  }
  if (m_bridge_shader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load bridge shader";
  }
  if (m_road_shader == nullptr) {
    qWarning() << "WaterPipeline: Failed to load road shader";
  }

  cache_uniforms();

  return is_initialized();
}

void WaterPipeline::shutdown() {
  m_river_shader = nullptr;
  m_riverbank_shader = nullptr;
  m_bridge_shader = nullptr;
  m_road_shader = nullptr;
}

void WaterPipeline::cache_uniforms() {
  cache_river_uniforms();
  cache_riverbank_uniforms();
  cache_bridge_uniforms();
  cache_road_uniforms();
}

auto WaterPipeline::is_initialized() const -> bool {
  return m_river_shader != nullptr && m_riverbank_shader != nullptr &&
         m_bridge_shader != nullptr && m_road_shader != nullptr;
}

void WaterPipeline::cache_river_uniforms() {
  if (m_river_shader == nullptr) {
    return;
  }

  m_river_uniforms.model = m_river_shader->uniform_handle("model");
  m_river_uniforms.view = m_river_shader->uniform_handle("view");
  m_river_uniforms.projection = m_river_shader->uniform_handle("projection");
  m_river_uniforms.time = m_river_shader->uniform_handle("time");
  m_river_uniforms.visibility_texture =
      m_river_shader->optional_uniform_handle("u_visibility_tex");
  m_river_uniforms.visibility_size =
      m_river_shader->optional_uniform_handle("u_visibility_size");
  m_river_uniforms.visibility_tile_size =
      m_river_shader->optional_uniform_handle("u_visibility_tile_size");
  m_river_uniforms.explored_alpha =
      m_river_shader->optional_uniform_handle("u_explored_alpha");
  m_river_uniforms.has_visibility =
      m_river_shader->optional_uniform_handle("u_has_visibility");
  m_river_uniforms.segment_visibility =
      m_river_shader->optional_uniform_handle("u_segment_visibility");
}

void WaterPipeline::cache_riverbank_uniforms() {
  if (m_riverbank_shader == nullptr) {
    return;
  }

  m_riverbank_uniforms.model = m_riverbank_shader->uniform_handle("model");
  m_riverbank_uniforms.view = m_riverbank_shader->uniform_handle("view");
  m_riverbank_uniforms.projection = m_riverbank_shader->uniform_handle("projection");
  m_riverbank_uniforms.time = m_riverbank_shader->uniform_handle("time");
  m_riverbank_uniforms.visibility_texture =
      m_riverbank_shader->uniform_handle("u_visibility_tex");
  m_riverbank_uniforms.visibility_size =
      m_riverbank_shader->uniform_handle("u_visibility_size");
  m_riverbank_uniforms.visibility_tile_size =
      m_riverbank_shader->uniform_handle("u_visibility_tile_size");
  m_riverbank_uniforms.explored_alpha =
      m_riverbank_shader->uniform_handle("u_explored_alpha");
  m_riverbank_uniforms.has_visibility =
      m_riverbank_shader->uniform_handle("u_has_visibility");
  m_riverbank_uniforms.segment_visibility =
      m_riverbank_shader->uniform_handle("u_segment_visibility");
}

void WaterPipeline::cache_bridge_uniforms() {
  if (m_bridge_shader == nullptr) {
    return;
  }

  m_bridge_uniforms.mvp = m_bridge_shader->uniform_handle("u_mvp");
  m_bridge_uniforms.model = m_bridge_shader->uniform_handle("u_model");
  m_bridge_uniforms.color = m_bridge_shader->uniform_handle("u_color");
  m_bridge_uniforms.light_direction =
      m_bridge_shader->uniform_handle("u_light_direction");
}

void WaterPipeline::cache_road_uniforms() {
  if (m_road_shader == nullptr) {
    return;
  }

  m_road_uniforms.mvp = m_road_shader->uniform_handle("u_mvp");
  m_road_uniforms.model = m_road_shader->uniform_handle("u_model");
  m_road_uniforms.color = m_road_shader->uniform_handle("u_color");
  m_road_uniforms.light_direction = m_road_shader->uniform_handle("u_light_direction");
  m_road_uniforms.alpha = m_road_shader->uniform_handle("u_alpha");
  m_road_uniforms.visibility_texture =
      m_road_shader->optional_uniform_handle("u_visibility_tex");
  m_road_uniforms.visibility_size =
      m_road_shader->optional_uniform_handle("u_visibility_size");
  m_road_uniforms.visibility_tile_size =
      m_road_shader->optional_uniform_handle("u_visibility_tile_size");
  m_road_uniforms.explored_alpha =
      m_road_shader->optional_uniform_handle("u_explored_alpha");
  m_road_uniforms.has_visibility =
      m_road_shader->optional_uniform_handle("u_has_visibility");
}

} // namespace Render::GL::BackendPipelines
