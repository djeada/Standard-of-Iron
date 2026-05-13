#include "terrain_pipeline.h"
#include "../backend.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLExtraFunctions>
#include <cstddef>
#include <qglobal.h>
#include <qopenglext.h>
#include <qvectornd.h>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

auto TerrainPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "TerrainPipeline::initialize: null ShaderCache";
    return false;
  }

  auto *gl = QOpenGLContext::currentContext()->extraFunctions();

  m_grass_shader = m_shader_cache->get("grass_instanced");
  m_ground_shader = m_shader_cache->get("ground_plane");
  m_terrain_shader = m_shader_cache->get("terrain_chunk");

  if (m_grass_shader == nullptr) {
    qWarning() << "TerrainPipeline: Failed to load grass_instanced shader";
  }
  if (m_ground_shader == nullptr) {
    qWarning() << "TerrainPipeline: Failed to load ground_plane shader";
  }
  if (m_terrain_shader == nullptr) {
    qWarning() << "TerrainPipeline: Failed to load basic (terrain) shader";
  }

  initialize_grass_geometry();

  cache_uniforms();

  return is_initialized();
}

void TerrainPipeline::shutdown() {
  shutdown_grass_geometry();
  m_grass_shader = nullptr;
  m_ground_shader = nullptr;
  m_terrain_shader = nullptr;
}

void TerrainPipeline::cache_uniforms() {
  cache_grass_uniforms();
  cache_ground_uniforms();
  cache_terrain_uniforms();
}

auto TerrainPipeline::is_initialized() const -> bool {
  return m_grass_shader != nullptr && m_ground_shader != nullptr &&
         m_terrain_shader != nullptr && m_grass_vao != 0;
}

void TerrainPipeline::cache_grass_uniforms() {
  if (m_grass_shader == nullptr) {
    return;
  }

  m_grass_uniforms.view_proj =
      m_grass_shader->optional_uniform_handle("u_view_proj");
  m_grass_uniforms.time = m_grass_shader->uniform_handle("u_time");
  m_grass_uniforms.wind_strength =
      m_grass_shader->uniform_handle("u_wind_strength");
  m_grass_uniforms.wind_speed = m_grass_shader->uniform_handle("u_wind_speed");
  m_grass_uniforms.soil_color = m_grass_shader->uniform_handle("u_soil_color");
  m_grass_uniforms.light_dir = m_grass_shader->uniform_handle("u_light_dir");
}

void TerrainPipeline::cache_ground_uniforms() {
  if (m_ground_shader == nullptr) {
    return;
  }

  m_ground_uniforms.mvp = m_ground_shader->uniform_handle("u_mvp");
  m_ground_uniforms.model = m_ground_shader->uniform_handle("u_model");
  m_ground_uniforms.grass_primary =
      m_ground_shader->uniform_handle("u_grass_primary");
  m_ground_uniforms.grass_secondary =
      m_ground_shader->uniform_handle("u_grass_secondary");
  m_ground_uniforms.grass_dry = m_ground_shader->uniform_handle("u_grass_dry");
  m_ground_uniforms.soil_color =
      m_ground_shader->uniform_handle("u_soil_color");
  m_ground_uniforms.rock_low = m_ground_shader->uniform_handle("u_rock_low");
  m_ground_uniforms.rock_high = m_ground_shader->uniform_handle("u_rock_high");
  m_ground_uniforms.tint = m_ground_shader->uniform_handle("u_tint");
  m_ground_uniforms.noise_offset =
      m_ground_shader->uniform_handle("u_noise_offset");
  m_ground_uniforms.noise_angle =
      m_ground_shader->uniform_handle("u_noise_angle");
  m_ground_uniforms.tile_size = m_ground_shader->uniform_handle("u_tile_size");
  m_ground_uniforms.macro_noise_scale =
      m_ground_shader->uniform_handle("u_macro_noise_scale");
  m_ground_uniforms.detail_noise_scale =
      m_ground_shader->uniform_handle("u_detail_noise_scale");
  m_ground_uniforms.soil_blend_height =
      m_ground_shader->uniform_handle("u_soil_blend_height");
  m_ground_uniforms.soil_blend_sharpness =
      m_ground_shader->uniform_handle("u_soil_blend_sharpness");
  m_ground_uniforms.height_noise_strength =
      m_ground_shader->uniform_handle("u_height_noise_strength");
  m_ground_uniforms.height_noise_frequency =
      m_ground_shader->uniform_handle("u_height_noise_frequency");
  m_ground_uniforms.ambient_boost =
      m_ground_shader->uniform_handle("u_ambient_boost");
  m_ground_uniforms.light_dir = m_ground_shader->uniform_handle("u_light_dir");

  m_ground_uniforms.snow_coverage =
      m_ground_shader->uniform_handle("u_snow_coverage");
  m_ground_uniforms.moisture_level =
      m_ground_shader->uniform_handle("u_moisture_level");
  m_ground_uniforms.crack_intensity =
      m_ground_shader->uniform_handle("u_crack_intensity");
  m_ground_uniforms.rock_exposure =
      m_ground_shader->uniform_handle("u_rock_exposure");
  m_ground_uniforms.grass_saturation =
      m_ground_shader->uniform_handle("u_grass_saturation");
  m_ground_uniforms.soil_roughness =
      m_ground_shader->uniform_handle("u_soil_roughness");
  m_ground_uniforms.micro_bump_amp =
      m_ground_shader->uniform_handle("u_micro_bump_amp");
  m_ground_uniforms.micro_bump_freq =
      m_ground_shader->uniform_handle("u_micro_bump_freq");
  m_ground_uniforms.micro_normal_weight =
      m_ground_shader->uniform_handle("u_micro_normal_weight");
  m_ground_uniforms.albedo_jitter =
      m_ground_shader->uniform_handle("u_albedo_jitter");
  m_ground_uniforms.snow_color =
      m_ground_shader->uniform_handle("u_snow_color");
  m_ground_uniforms.camera_position =
      m_ground_shader->uniform_handle("u_camera_pos");
  m_ground_uniforms.fog_color = m_ground_shader->uniform_handle("u_fog_color");
  m_ground_uniforms.fog_start = m_ground_shader->uniform_handle("u_fog_start");
  m_ground_uniforms.fog_end = m_ground_shader->uniform_handle("u_fog_end");
}

void TerrainPipeline::cache_terrain_uniforms() {
  if (m_terrain_shader == nullptr) {
    return;
  }

  m_terrain_uniforms.mvp = m_terrain_shader->uniform_handle("u_mvp");
  m_terrain_uniforms.model = m_terrain_shader->uniform_handle("u_model");
  m_terrain_uniforms.grass_primary =
      m_terrain_shader->uniform_handle("u_grass_primary");
  m_terrain_uniforms.grass_secondary =
      m_terrain_shader->uniform_handle("u_grass_secondary");
  m_terrain_uniforms.grass_dry =
      m_terrain_shader->uniform_handle("u_grass_dry");
  m_terrain_uniforms.soil_color =
      m_terrain_shader->uniform_handle("u_soil_color");
  m_terrain_uniforms.rock_low = m_terrain_shader->uniform_handle("u_rock_low");
  m_terrain_uniforms.rock_high =
      m_terrain_shader->uniform_handle("u_rock_high");
  m_terrain_uniforms.tint = m_terrain_shader->uniform_handle("u_tint");
  m_terrain_uniforms.noise_offset =
      m_terrain_shader->uniform_handle("u_noise_offset");
  m_terrain_uniforms.tile_size =
      m_terrain_shader->uniform_handle("u_tile_size");
  m_terrain_uniforms.macro_noise_scale =
      m_terrain_shader->uniform_handle("u_macro_noise_scale");
  m_terrain_uniforms.detail_noise_scale =
      m_terrain_shader->uniform_handle("u_detail_noise_scale");
  m_terrain_uniforms.slope_rock_threshold =
      m_terrain_shader->uniform_handle("u_slope_rock_threshold");
  m_terrain_uniforms.slope_rock_sharpness =
      m_terrain_shader->uniform_handle("u_slope_rock_sharpness");
  m_terrain_uniforms.soil_blend_height =
      m_terrain_shader->uniform_handle("u_soil_blend_height");
  m_terrain_uniforms.soil_blend_sharpness =
      m_terrain_shader->uniform_handle("u_soil_blend_sharpness");
  m_terrain_uniforms.height_noise_strength =
      m_terrain_shader->uniform_handle("u_height_noise_strength");
  m_terrain_uniforms.height_noise_frequency =
      m_terrain_shader->uniform_handle("u_height_noise_frequency");
  m_terrain_uniforms.ambient_boost =
      m_terrain_shader->uniform_handle("u_ambient_boost");
  m_terrain_uniforms.rock_detail_strength =
      m_terrain_shader->uniform_handle("u_rock_detail_strength");
  m_terrain_uniforms.light_dir =
      m_terrain_shader->uniform_handle("u_light_dir");

  m_terrain_uniforms.snow_coverage =
      m_terrain_shader->uniform_handle("u_snow_coverage");
  m_terrain_uniforms.moisture_level =
      m_terrain_shader->uniform_handle("u_moisture_level");
  m_terrain_uniforms.crack_intensity =
      m_terrain_shader->uniform_handle("u_crack_intensity");
  m_terrain_uniforms.rock_exposure =
      m_terrain_shader->uniform_handle("u_rock_exposure");
  m_terrain_uniforms.grass_saturation =
      m_terrain_shader->uniform_handle("u_grass_saturation");
  m_terrain_uniforms.soil_roughness =
      m_terrain_shader->uniform_handle("u_soil_roughness");
  m_terrain_uniforms.curvature_response =
      m_terrain_shader->uniform_handle("u_curvature_response");
  m_terrain_uniforms.ridge_response =
      m_terrain_shader->uniform_handle("u_ridge_response");
  m_terrain_uniforms.gully_response =
      m_terrain_shader->uniform_handle("u_gully_response");
  m_terrain_uniforms.snow_color =
      m_terrain_shader->uniform_handle("u_snow_color");
  m_terrain_uniforms.soil_foot_height =
      m_terrain_shader->uniform_handle("u_soil_foot_height");
  m_terrain_uniforms.screen_toe_mul =
      m_terrain_shader->uniform_handle("u_screen_toe_mul");
  m_terrain_uniforms.screen_toe_clamp =
      m_terrain_shader->uniform_handle("u_screen_toe_clamp");
  m_terrain_uniforms.camera_position =
      m_terrain_shader->uniform_handle("u_camera_pos");
  m_terrain_uniforms.fog_color =
      m_terrain_shader->uniform_handle("u_fog_color");
  m_terrain_uniforms.fog_start =
      m_terrain_shader->uniform_handle("u_fog_start");
  m_terrain_uniforms.fog_end = m_terrain_shader->uniform_handle("u_fog_end");
}

void TerrainPipeline::initialize_grass_geometry() {
  auto *gl = QOpenGLContext::currentContext()->extraFunctions();
  if (gl == nullptr) {
    qWarning()
        << "TerrainPipeline::initialize_grass_geometry: no OpenGL context";
    return;
  }

  shutdown_grass_geometry();

  struct GrassVertex {
    QVector3D position;
    QVector2D uv;
  };

  const GrassVertex blade_vertices[6] = {
      {{-0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}},
      {{0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}},
      {{-0.35F, 1.0F, 0.0F}, {0.1F, 1.0F}},
      {{-0.35F, 1.0F, 0.0F}, {0.1F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}},
      {{0.35F, 1.0F, 0.0F}, {0.9F, 1.0F}},
  };

  gl->glGenVertexArrays(1, &m_grass_vao);
  gl->glBindVertexArray(m_grass_vao);

  gl->glGenBuffers(1, &m_grass_vertex_buffer);
  gl->glBindBuffer(GL_ARRAY_BUFFER, m_grass_vertex_buffer);
  gl->glBufferData(GL_ARRAY_BUFFER, sizeof(blade_vertices), blade_vertices,
                   GL_STATIC_DRAW);
  m_grass_vertex_count = grass_blade_vertex_count;

  gl->glEnableVertexAttribArray(position);
  gl->glVertexAttribPointer(
      position, vec3, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
      reinterpret_cast<void *>(offsetof(GrassVertex, position)));

  gl->glEnableVertexAttribArray(normal);
  gl->glVertexAttribPointer(
      normal, vec2, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
      reinterpret_cast<void *>(offsetof(GrassVertex, uv)));

  gl->glEnableVertexAttribArray(tex_coord);
  gl->glVertexAttribDivisor(tex_coord, 1);
  gl->glEnableVertexAttribArray(instance_position);
  gl->glVertexAttribDivisor(instance_position, 1);
  gl->glEnableVertexAttribArray(instance_scale);
  gl->glVertexAttribDivisor(instance_scale, 1);

  gl->glBindVertexArray(0);
  gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TerrainPipeline::shutdown_grass_geometry() {
  auto *gl = QOpenGLContext::currentContext()->extraFunctions();
  if (gl == nullptr) {
    return;
  }

  if (m_grass_vertex_buffer != 0U) {
    gl->glDeleteBuffers(1, &m_grass_vertex_buffer);
    m_grass_vertex_buffer = 0;
  }
  if (m_grass_vao != 0U) {
    gl->glDeleteVertexArrays(1, &m_grass_vao);
    m_grass_vao = 0;
  }
  m_grass_vertex_count = 0;
}

} // namespace Render::GL::BackendPipelines
