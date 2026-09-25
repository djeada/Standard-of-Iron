#include "terrain_pipeline.h"

#include <QDebug>
#include <QOpenGLExtraFunctions>
#include <qglobal.h>
#include <qopenglext.h>
#include <qvectornd.h>

#include <cmath>
#include <cstddef>
#include <numbers>

#include "render/gl/gl_resource_tracking.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

auto TerrainPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "TerrainPipeline::initialize: null ShaderCache";
    return false;
  }

  m_grass_shader = m_shader_cache->get("grass_instanced");
  m_ground_shader = m_shader_cache->get("ground_plane");
  m_terrain_shader = m_shader_cache->get("terrain_chunk");
  m_terrain_baked_shader = m_shader_cache->get("terrain_chunk_baked");

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
  m_terrain_baked_shader = nullptr;
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

  m_grass_uniforms.view_proj = m_grass_shader->optional_uniform_handle("u_view_proj");
  m_grass_uniforms.time = m_grass_shader->uniform_handle("u_time");
  m_grass_uniforms.wind_strength = m_grass_shader->uniform_handle("u_wind_strength");
  m_grass_uniforms.wind_speed = m_grass_shader->uniform_handle("u_wind_speed");
  m_grass_uniforms.soil_color = m_grass_shader->uniform_handle("u_soil_color");
  m_grass_uniforms.light_dir = m_grass_shader->optional_uniform_handle("u_light_dir");
  m_grass_uniforms.viewport_size =
      m_grass_shader->optional_uniform_handle("u_viewport_size");
  m_grass_uniforms.camera_pos = m_grass_shader->optional_uniform_handle("u_camera_pos");
  m_grass_uniforms.ambient_boost =
      m_grass_shader->optional_uniform_handle("u_ambient_boost");
}

void TerrainPipeline::cache_ground_uniforms() {
  if (m_ground_shader == nullptr) {
    return;
  }

  m_ground_uniforms.mvp = m_ground_shader->uniform_handle("u_mvp");
  m_ground_uniforms.model = m_ground_shader->uniform_handle("u_model");
  m_ground_uniforms.ground_type =
      m_ground_shader->optional_uniform_handle("u_ground_type");
  m_ground_uniforms.grass_primary = m_ground_shader->uniform_handle("u_grass_primary");
  m_ground_uniforms.grass_secondary =
      m_ground_shader->uniform_handle("u_grass_secondary");
  m_ground_uniforms.grass_dry = m_ground_shader->uniform_handle("u_grass_dry");
  m_ground_uniforms.soil_color = m_ground_shader->uniform_handle("u_soil_color");
  m_ground_uniforms.rock_low = m_ground_shader->uniform_handle("u_rock_low");
  m_ground_uniforms.rock_high = m_ground_shader->uniform_handle("u_rock_high");
  m_ground_uniforms.tint = m_ground_shader->uniform_handle("u_tint");
  m_ground_uniforms.noise_offset = m_ground_shader->uniform_handle("u_noise_offset");
  m_ground_uniforms.noise_angle = m_ground_shader->uniform_handle("u_noise_angle");
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
  m_ground_uniforms.ambient_boost = m_ground_shader->uniform_handle("u_ambient_boost");
  m_ground_uniforms.light_dir = m_ground_shader->optional_uniform_handle("u_light_dir");

  m_ground_uniforms.snow_coverage = m_ground_shader->uniform_handle("u_snow_coverage");
  m_ground_uniforms.moisture_level =
      m_ground_shader->uniform_handle("u_moisture_level");
  m_ground_uniforms.crack_intensity =
      m_ground_shader->uniform_handle("u_crack_intensity");
  m_ground_uniforms.rock_exposure = m_ground_shader->uniform_handle("u_rock_exposure");
  m_ground_uniforms.grass_saturation =
      m_ground_shader->uniform_handle("u_grass_saturation");
  m_ground_uniforms.soil_roughness =
      m_ground_shader->uniform_handle("u_soil_roughness");

  m_ground_uniforms.micro_bump_amp =
      m_ground_shader->optional_uniform_handle("u_micro_bump_amp");
  m_ground_uniforms.micro_bump_freq =
      m_ground_shader->optional_uniform_handle("u_micro_bump_freq");
  m_ground_uniforms.micro_normal_weight =
      m_ground_shader->uniform_handle("u_micro_normal_weight");
  m_ground_uniforms.albedo_jitter = m_ground_shader->uniform_handle("u_albedo_jitter");
  m_ground_uniforms.snow_color = m_ground_shader->uniform_handle("u_snow_color");
  m_ground_uniforms.camera_position = m_ground_shader->uniform_handle("u_camera_pos");
}

void TerrainPipeline::cache_terrain_uniforms() {
  cache_terrain_uniforms(m_terrain_shader, m_terrain_uniforms, false);
  cache_terrain_uniforms(m_terrain_baked_shader, m_terrain_baked_uniforms, true);
}

void TerrainPipeline::cache_terrain_uniforms(GL::Shader* shader,
                                             TerrainUniforms& uniforms,
                                             bool all_optional) {
  if (shader == nullptr) {
    return;
  }
  auto required = [shader, all_optional](const char* name) {
    return all_optional ? shader->optional_uniform_handle(name)
                        : shader->uniform_handle(name);
  };

  uniforms.mvp = required("u_mvp");
  uniforms.model = required("u_model");
  uniforms.ground_type = shader->optional_uniform_handle("u_ground_type");
  uniforms.terrain_type = shader->optional_uniform_handle("u_terrain_type");
  uniforms.grass_primary = required("u_grass_primary");
  uniforms.grass_secondary = required("u_grass_secondary");
  uniforms.grass_dry = required("u_grass_dry");
  uniforms.soil_color = required("u_soil_color");
  uniforms.rock_low = required("u_rock_low");
  uniforms.rock_high = required("u_rock_high");
  uniforms.tint = required("u_tint");
  uniforms.noise_offset = required("u_noise_offset");
  uniforms.tile_size = required("u_tile_size");
  uniforms.macro_noise_scale = required("u_macro_noise_scale");
  uniforms.detail_noise_scale = shader->optional_uniform_handle("u_detail_noise_scale");
  uniforms.slope_rock_threshold = required("u_slope_rock_threshold");
  uniforms.slope_rock_sharpness =
      shader->optional_uniform_handle("u_slope_rock_sharpness");
  uniforms.soil_blend_height = shader->optional_uniform_handle("u_soil_blend_height");
  uniforms.soil_blend_sharpness =
      shader->optional_uniform_handle("u_soil_blend_sharpness");
  uniforms.height_noise_strength = required("u_height_noise_strength");
  uniforms.height_noise_frequency = required("u_height_noise_frequency");
  uniforms.ambient_boost = required("u_ambient_boost");
  uniforms.rock_detail_strength =
      shader->optional_uniform_handle("u_rock_detail_strength");
  uniforms.light_dir = shader->optional_uniform_handle("u_light_dir");

  uniforms.snow_coverage = required("u_snow_coverage");
  uniforms.moisture_level = required("u_moisture_level");
  uniforms.crack_intensity = required("u_crack_intensity");
  uniforms.rock_exposure = required("u_rock_exposure");
  uniforms.grass_saturation = required("u_grass_saturation");
  uniforms.soil_roughness = shader->optional_uniform_handle("u_soil_roughness");
  uniforms.curvature_response = required("u_curvature_response");
  uniforms.ridge_response = required("u_ridge_response");
  uniforms.gully_response = required("u_gully_response");
  uniforms.snow_color = required("u_snow_color");
  uniforms.soil_foot_height = shader->optional_uniform_handle("u_soil_foot_height");
  uniforms.screen_toe_mul = shader->optional_uniform_handle("u_screen_toe_mul");
  uniforms.screen_toe_clamp = shader->optional_uniform_handle("u_screen_toe_clamp");
  uniforms.has_height_texture = shader->optional_uniform_handle("u_has_height_tex");
  uniforms.height_texture = shader->optional_uniform_handle("u_height_tex");
  uniforms.has_field_texture = shader->optional_uniform_handle("u_has_field_tex");
  uniforms.has_cover_texture = shader->optional_uniform_handle("u_has_cover_tex");
  uniforms.has_noise_atlas = shader->optional_uniform_handle("u_has_noise_atlas");
  uniforms.noise_atlas = shader->optional_uniform_handle("u_noise_atlas");
  uniforms.noise_atlas_detail = shader->optional_uniform_handle("u_noise_atlas_detail");
  uniforms.local_light_mask = shader->optional_uniform_handle("u_local_light_mask");
  uniforms.has_local_light_mask =
      shader->optional_uniform_handle("u_has_local_light_mask");
  uniforms.has_microdetail = shader->optional_uniform_handle("u_has_microdetail");
  uniforms.microdetail = shader->optional_uniform_handle("u_microdetail");
  uniforms.noise_atlas_world_size =
      shader->optional_uniform_handle("u_noise_atlas_world_size");
  uniforms.field_texture = shader->optional_uniform_handle("u_field_tex");
  uniforms.cover_texture = shader->optional_uniform_handle("u_cover_tex");
  uniforms.height_texel_size = shader->optional_uniform_handle("u_height_texel_size");
  uniforms.height_uv_scale = shader->optional_uniform_handle("u_height_uv_scale");
  uniforms.height_uv_offset = shader->optional_uniform_handle("u_height_uv_offset");
  uniforms.height_to_world = shader->optional_uniform_handle("u_height_tex_to_world");
  uniforms.camera_position = required("u_camera_pos");
  uniforms.has_visibility = shader->optional_uniform_handle("u_has_visibility");
  uniforms.visibility_texture = shader->optional_uniform_handle("u_visibility_tex");
  uniforms.visibility_size = shader->optional_uniform_handle("u_visibility_size");
  uniforms.visibility_tile_size =
      shader->optional_uniform_handle("u_visibility_tile_size");
  uniforms.explored_alpha = shader->optional_uniform_handle("u_explored_alpha");
}

void TerrainPipeline::initialize_grass_geometry() {
  auto* gl = QOpenGLContext::currentContext()->extraFunctions();
  if (gl == nullptr) {
    qWarning() << "TerrainPipeline::initialize_grass_geometry: no OpenGL context";
    return;
  }

  shutdown_grass_geometry();

  struct GrassVertex {
    QVector3D position;
    QVector2D uv;
  };

  GrassVertex blade_vertices[grass_blade_vertex_count];

  constexpr int k_rings = grass_blade_segments + 1;
  constexpr float k_ring_height[k_rings] = {0.0F, 0.56F, 1.0F};
  constexpr float k_ring_half_width[k_rings] = {0.50F, 0.33F, 0.07F};
  constexpr float k_ring_splay[k_rings] = {0.0F, 0.30F, 1.12F};
  constexpr float k_blade_height_scale[grass_blades_per_tuft] = {1.0F, 0.82F, 0.94F};
  constexpr float k_blade_splay_scale[grass_blades_per_tuft] = {1.0F, 1.32F, 0.72F};

  for (int blade = 0; blade < grass_blades_per_tuft; ++blade) {
    const float angle = (2.0F * std::numbers::pi_v<float> * static_cast<float>(blade)) /
                        static_cast<float>(grass_blades_per_tuft);
    const QVector2D right(std::cos(angle), std::sin(angle));
    const QVector2D splay(-std::sin(angle), std::cos(angle));
    const float height = k_blade_height_scale[blade];
    const float splay_scale = k_blade_splay_scale[blade];

    auto ring_vertex = [&](int ring, float side, float u) {
      const QVector2D plane = (right * (side * k_ring_half_width[ring])) +
                              (splay * (k_ring_splay[ring] * splay_scale));
      GrassVertex out;
      out.position = QVector3D(plane.x(), k_ring_height[ring] * height, plane.y());
      out.uv = QVector2D(u, k_ring_height[ring]);
      return out;
    };

    for (int segment = 0; segment < grass_blade_segments; ++segment) {
      const int lower = segment;
      const int upper = segment + 1;
      const int base_index = (blade * grass_blade_segments + segment) * 6;
      blade_vertices[base_index + 0] = ring_vertex(lower, -1.0F, 0.0F);
      blade_vertices[base_index + 1] = ring_vertex(lower, 1.0F, 1.0F);
      blade_vertices[base_index + 2] = ring_vertex(upper, -1.0F, 0.0F);
      blade_vertices[base_index + 3] = ring_vertex(upper, -1.0F, 0.0F);
      blade_vertices[base_index + 4] = ring_vertex(lower, 1.0F, 1.0F);
      blade_vertices[base_index + 5] = ring_vertex(upper, 1.0F, 1.0F);
    }
  }

  gl->glGenVertexArrays(1, &m_grass_vao);
  note_vertex_arrays_created(1);
  gl->glBindVertexArray(m_grass_vao);

  gl->glGenBuffers(1, &m_grass_vertex_buffer);
  note_buffers_created(1);
  gl->glBindBuffer(GL_ARRAY_BUFFER, m_grass_vertex_buffer);
  gl->glBufferData(
      GL_ARRAY_BUFFER, sizeof(blade_vertices), blade_vertices, GL_STATIC_DRAW);
  note_buffer_storage(static_cast<std::size_t>(sizeof(blade_vertices)),
                      blade_vertices != nullptr);
  m_grass_vertex_count = grass_blade_vertex_count;

  gl->glEnableVertexAttribArray(position);
  gl->glVertexAttribPointer(position,
                            vec3,
                            GL_FLOAT,
                            GL_FALSE,
                            sizeof(GrassVertex),
                            reinterpret_cast<void*>(offsetof(GrassVertex, position)));

  gl->glEnableVertexAttribArray(normal);
  gl->glVertexAttribPointer(normal,
                            vec2,
                            GL_FLOAT,
                            GL_FALSE,
                            sizeof(GrassVertex),
                            reinterpret_cast<void*>(offsetof(GrassVertex, uv)));

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
  auto* gl = QOpenGLContext::currentContext()->extraFunctions();
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
