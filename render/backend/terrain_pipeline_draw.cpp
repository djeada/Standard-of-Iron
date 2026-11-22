#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "render/backend/pipeline_draw_context.h"
#include "render/backend/state_scopes.h"
#include "render/backend/terrain_pipeline.h"
#include "render/backend/visibility_mask_binding.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_commands.h"
#include "render/frame/draw_queue.h"
#include "render/frame/local_lighting.h"
#include "render/gl/buffer.h"
#include "render/gl/draw_tally.h"
#include "render/gl/mesh.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "render/gl/texture.h"
#include "render/gl/vertex_attrib_layout.h"
#include "scene/camera.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

template <typename Uniforms>
void set_camera_uniform(Shader& shader,
                        const Uniforms& uniforms,
                        const QVector3D& camera_position) {
  if (uniforms.camera_position != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.camera_position, camera_position);
  }
}

} // namespace

void TerrainPipeline::set_ground_plane_uniforms(Shader& shader,
                                                const TerrainSurfaceCmd& single,
                                                const QMatrix4x4& mvp,
                                                const QVector3D& camera_position) {
  if (m_ground_uniforms.mvp != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.mvp, mvp);
  }
  if (m_ground_uniforms.model != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.model, single.model);
  }
  if (m_ground_uniforms.ground_type != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.ground_type, single.params.ground_type);
  }
  if (m_ground_uniforms.grass_primary != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.grass_primary, single.params.grass_primary);
  }
  if (m_ground_uniforms.grass_secondary != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.grass_secondary,
                       single.params.grass_secondary);
  }
  if (m_ground_uniforms.grass_dry != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.grass_dry, single.params.grass_dry);
  }
  if (m_ground_uniforms.soil_color != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.soil_color, single.params.soil_color);
  }
  if (m_ground_uniforms.rock_low != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.rock_low, single.params.rock_low);
  }
  if (m_ground_uniforms.rock_high != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.rock_high, single.params.rock_high);
  }
  if (m_ground_uniforms.tint != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.tint, single.params.tint);
  }
  if (m_ground_uniforms.noise_offset != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.noise_offset, single.params.noise_offset);
  }
  if (m_ground_uniforms.noise_angle != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.noise_angle, single.params.noise_angle);
  }
  if (m_ground_uniforms.tile_size != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.tile_size, single.params.tile_size);
  }
  if (m_ground_uniforms.macro_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.macro_noise_scale,
                       single.params.macro_noise_scale);
  }
  if (m_ground_uniforms.detail_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.detail_noise_scale,
                       single.params.detail_noise_scale);
  }
  if (m_ground_uniforms.soil_blend_height != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.soil_blend_height,
                       single.params.soil_blend_height);
  }
  if (m_ground_uniforms.soil_blend_sharpness != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.soil_blend_sharpness,
                       single.params.soil_blend_sharpness);
  }
  if (m_ground_uniforms.height_noise_strength != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.height_noise_strength,
                       single.params.height_noise_strength);
  }
  if (m_ground_uniforms.height_noise_frequency != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.height_noise_frequency,
                       single.params.height_noise_frequency);
  }
  if (m_ground_uniforms.ambient_boost != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.ambient_boost, single.params.ambient_boost);
  }
  if (m_ground_uniforms.light_dir != Shader::InvalidUniform) {
    QVector3D light_dir = single.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    shader.set_uniform(m_ground_uniforms.light_dir, light_dir);
  }
  if (m_ground_uniforms.snow_coverage != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.snow_coverage, single.params.snow_coverage);
  }
  if (m_ground_uniforms.moisture_level != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.moisture_level, single.params.moisture_level);
  }
  if (m_ground_uniforms.crack_intensity != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.crack_intensity,
                       single.params.crack_intensity);
  }
  if (m_ground_uniforms.rock_exposure != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.rock_exposure, single.params.rock_exposure);
  }
  if (m_ground_uniforms.grass_saturation != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.grass_saturation,
                       single.params.grass_saturation);
  }
  if (m_ground_uniforms.soil_roughness != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.soil_roughness, single.params.soil_roughness);
  }
  if (m_ground_uniforms.micro_bump_amp != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.micro_bump_amp, single.params.micro_bump_amp);
  }
  if (m_ground_uniforms.micro_bump_freq != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.micro_bump_freq,
                       single.params.micro_bump_freq);
  }
  if (m_ground_uniforms.micro_normal_weight != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.micro_normal_weight,
                       single.params.micro_normal_weight);
  }
  if (m_ground_uniforms.albedo_jitter != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.albedo_jitter, single.params.albedo_jitter);
  }
  if (m_ground_uniforms.snow_color != Shader::InvalidUniform) {
    shader.set_uniform(m_ground_uniforms.snow_color, single.params.snow_color);
  }
  set_camera_uniform(shader, m_ground_uniforms, camera_position);
}

void TerrainPipeline::set_terrain_chunk_uniforms(
    Shader& shader,
    const TerrainSurfaceCmd& single,
    const QMatrix4x4& mvp,
    const QVector3D& camera_position,
    PipelineDrawContext& context,
    const std::vector<Render::LocalLight>& local_lights) {
  const auto& uniforms = terrain_uniforms_for(shader);
  const auto& visibility = single.visibility;
  if (uniforms.has_visibility != Shader::InvalidUniform) {
    int const has_vis = visibility.enabled && (visibility.texture != nullptr) ? 1 : 0;
    shader.set_uniform(uniforms.has_visibility, has_vis);
  }
  if (visibility.enabled && visibility.texture != nullptr) {
    if (uniforms.visibility_size != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.visibility_size, visibility.size);
    }
    if (uniforms.visibility_tile_size != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.visibility_tile_size, visibility.tile_size);
    }
    if (uniforms.explored_alpha != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.explored_alpha, visibility.explored_alpha);
    }
    visibility.texture->bind(TextureUnit::terrain_visibility);
    context.state.bound_texture = visibility.texture;
    if (uniforms.visibility_texture != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.visibility_texture, TextureUnit::terrain_visibility);
    }
  }
  if (uniforms.mvp != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.mvp, mvp);
  }
  if (uniforms.model != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.model, single.model);
  }
  if (uniforms.ground_type != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.ground_type, single.params.ground_type);
  }
  if (uniforms.terrain_type != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.terrain_type, single.params.terrain_type);
  }
  if (uniforms.grass_primary != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.grass_primary, single.params.grass_primary);
  }
  if (uniforms.grass_secondary != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.grass_secondary, single.params.grass_secondary);
  }
  if (uniforms.grass_dry != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.grass_dry, single.params.grass_dry);
  }
  if (uniforms.soil_color != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.soil_color, single.params.soil_color);
  }
  if (uniforms.rock_low != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.rock_low, single.params.rock_low);
  }
  if (uniforms.rock_high != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.rock_high, single.params.rock_high);
  }
  if (uniforms.tint != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.tint, single.params.tint);
  }
  if (uniforms.noise_offset != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.noise_offset, single.params.noise_offset);
  }
  if (uniforms.tile_size != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.tile_size, single.params.tile_size);
  }
  if (uniforms.macro_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.macro_noise_scale, single.params.macro_noise_scale);
  }
  if (uniforms.detail_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.detail_noise_scale, single.params.detail_noise_scale);
  }
  if (uniforms.slope_rock_threshold != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.slope_rock_threshold,
                       single.params.slope_rock_threshold);
  }
  if (uniforms.slope_rock_sharpness != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.slope_rock_sharpness,
                       single.params.slope_rock_sharpness);
  }
  if (uniforms.soil_blend_height != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.soil_blend_height, single.params.soil_blend_height);
  }
  if (uniforms.soil_blend_sharpness != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.soil_blend_sharpness,
                       single.params.soil_blend_sharpness);
  }
  if (uniforms.height_noise_strength != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.height_noise_strength,
                       single.params.height_noise_strength);
  }
  if (uniforms.height_noise_frequency != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.height_noise_frequency,
                       single.params.height_noise_frequency);
  }
  if (uniforms.ambient_boost != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.ambient_boost, single.params.ambient_boost);
  }
  if (uniforms.rock_detail_strength != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.rock_detail_strength,
                       single.params.rock_detail_strength);
  }
  if (uniforms.horizon_dressing != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.horizon_dressing,
                       single.horizon_dressing ? 1.0F : 0.0F);
  }
  if (uniforms.light_dir != Shader::InvalidUniform) {
    QVector3D light_dir = single.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    shader.set_uniform(uniforms.light_dir, light_dir);
  }
  if (uniforms.snow_coverage != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.snow_coverage, single.params.snow_coverage);
  }
  if (uniforms.moisture_level != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.moisture_level, single.params.moisture_level);
  }
  if (uniforms.crack_intensity != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.crack_intensity, single.params.crack_intensity);
  }
  if (uniforms.rock_exposure != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.rock_exposure, single.params.rock_exposure);
  }
  if (uniforms.grass_saturation != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.grass_saturation, single.params.grass_saturation);
  }
  if (uniforms.soil_roughness != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.soil_roughness, single.params.soil_roughness);
  }
  if (uniforms.curvature_response != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.curvature_response, single.params.curvature_response);
  }
  if (uniforms.ridge_response != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.ridge_response, single.params.ridge_response);
  }
  if (uniforms.gully_response != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.gully_response, single.params.gully_response);
  }
  if (uniforms.snow_color != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.snow_color, single.params.snow_color);
  }
  if (uniforms.soil_foot_height != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.soil_foot_height, single.params.soil_foot_height);
  }
  if (uniforms.screen_toe_mul != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.screen_toe_mul, single.params.screen_toe_mul);
  }
  if (uniforms.screen_toe_clamp != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.screen_toe_clamp, single.params.screen_toe_clamp);
  }
  const auto& height = single.height;
  if (uniforms.has_height_texture != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.has_height_texture,
                       height.enabled && height.texture != nullptr ? 1 : 0);
  }
  if (height.enabled && height.texture != nullptr) {
    height.texture->bind(TextureUnit::terrain_height);
    context.state.bound_texture = height.texture;
    if (uniforms.height_texture != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.height_texture, TextureUnit::terrain_height);
    }
    if (uniforms.height_texel_size != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.height_texel_size, height.texel_size);
    }
    if (uniforms.height_uv_scale != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.height_uv_scale, height.uv_scale);
    }
    if (uniforms.height_uv_offset != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.height_uv_offset, height.uv_offset);
    }
    if (uniforms.height_to_world != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.height_to_world, height.to_world);
    }
  }
  const bool field_ready = height.enabled && height.field_texture != nullptr;
  if (uniforms.has_field_texture != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.has_field_texture, field_ready ? 1 : 0);
  }
  if (field_ready) {
    height.field_texture->bind(TextureUnit::terrain_fields);
    if (uniforms.field_texture != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.field_texture, TextureUnit::terrain_fields);
    }
  }
  const bool cover_ready = height.enabled && height.cover_texture != nullptr;
  if (uniforms.has_cover_texture != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.has_cover_texture, cover_ready ? 1 : 0);
  }
  if (cover_ready) {
    context.frame.surface.ground_cover = {
        height.cover_texture, height.uv_scale, height.uv_offset};
    height.cover_texture->bind(TextureUnit::terrain_cover);
    if (uniforms.cover_texture != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.cover_texture, TextureUnit::terrain_cover);
    }
  }

  const bool atlas_ready = height.noise_atlas != 0U && height.noise_atlas_detail != 0U;
  if (uniforms.has_noise_atlas != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.has_noise_atlas, atlas_ready ? 1 : 0);
  }
  if (atlas_ready) {
    glActiveTexture(GL_TEXTURE0 + TextureUnit::terrain_noise_atlas);
    glBindTexture(GL_TEXTURE_2D, height.noise_atlas);
    glActiveTexture(GL_TEXTURE0);
    if (uniforms.noise_atlas != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.noise_atlas, TextureUnit::terrain_noise_atlas);
    }
    glActiveTexture(GL_TEXTURE0 + TextureUnit::terrain_noise_atlas_detail);
    glBindTexture(GL_TEXTURE_2D, height.noise_atlas_detail);
    glActiveTexture(GL_TEXTURE0);
    if (uniforms.noise_atlas_detail != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.noise_atlas_detail,
                         TextureUnit::terrain_noise_atlas_detail);
    }
    if (uniforms.noise_atlas_world_size != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.noise_atlas_world_size,
                         height.noise_atlas_world_size);
    }
  }
  const bool microdetail_ready = height.microdetail != 0U;
  if (uniforms.has_microdetail != Shader::InvalidUniform) {
    shader.set_uniform(uniforms.has_microdetail, microdetail_ready ? 1 : 0);
  }
  if (microdetail_ready) {
    glActiveTexture(GL_TEXTURE0 + TextureUnit::terrain_microdetail);
    glBindTexture(GL_TEXTURE_2D, height.microdetail);
    glActiveTexture(GL_TEXTURE0);
    if (uniforms.microdetail != Shader::InvalidUniform) {
      shader.set_uniform(uniforms.microdetail, TextureUnit::terrain_microdetail);
    }
  }

  if (uniforms.has_local_light_mask != Shader::InvalidUniform) {
    const bool mask_ready = !local_lights.empty();
    shader.set_uniform(uniforms.has_local_light_mask, mask_ready ? 1 : 0);
    if (mask_ready && uniforms.local_light_mask != Shader::InvalidUniform) {
      const QVector3D center = single.aabb.center();
      const QVector3D extents = single.aabb.extents();
      const auto mask =
          Render::local_light_mask_for_bounds(local_lights, center, extents.length());
      shader.set_uniform(uniforms.local_light_mask, static_cast<int>(mask));
    }
  }

  set_camera_uniform(shader, uniforms, camera_position);
}

auto TerrainPipeline::surface_shader_for(const TerrainSurfaceCmd& single) const
    -> Shader* {
  if (single.params.is_ground_plane) {
    return m_ground_shader;
  }
  const auto& height = single.height;
  if (m_terrain_baked_shader != nullptr && height.noise_atlas != 0U &&
      height.noise_atlas_detail != 0U && height.microdetail != 0U) {
    return m_terrain_baked_shader;
  }
  return m_terrain_shader;
}

void TerrainPipeline::draw_surfaces(
    const PreparedBatch& prepared,
    PipelineDrawContext& context,
    const std::vector<Render::LocalLight>& local_lights) {
  const auto& queue = context.queue;
  const QMatrix4x4& view_proj = context.frame.view_proj;

  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();
  const auto& terrain = std::get<TerrainSurfaceCmdIndex>(queue.get_sorted(i));

  if ((terrain.mesh == nullptr) || (surface_shader_for(terrain) == nullptr)) {
    return;
  }

  QVector3D const camera_position = context.frame.camera->get_position();

  auto draw_surface = [&](const TerrainSurfaceCmd& single) {
    Shader* active_shader = surface_shader_for(single);
    if (active_shader == nullptr) {
      return;
    }
    if (context.state.bound_shader != active_shader) {
      active_shader->use();
      context.state.bound_shader = active_shader;
      context.state.bound_texture = nullptr;
    }
    const QMatrix4x4 mvp = view_proj * single.model;
    if (single.params.is_ground_plane) {
      set_ground_plane_uniforms(*active_shader, single, mvp, camera_position);
    } else {
      set_terrain_chunk_uniforms(
          *active_shader, single, mvp, camera_position, context, local_lights);
    }

    if (single.depth_bias != 0.0F) {
      PolygonOffsetScope const poly_scope(single.depth_bias, single.depth_bias);
      single.mesh->draw();
    } else {
      single.mesh->draw();
    }
  };

  DepthMaskScope const depth_mask(terrain.depth_write);
  std::optional<PolygonModeScope> polygon_mode_scope;
  if (terrain.wireframe) {
    polygon_mode_scope.emplace(GL_LINE);
  }
  for (std::size_t j = i; j < batch_end; ++j) {
    const auto& single = std::get<TerrainSurfaceCmdIndex>(queue.get_sorted(j));
    draw_surface(single);
  }
}

void TerrainPipeline::draw_grass(const TerrainScatterCmd& deco_cmd_,
                                 PipelineDrawContext& context) {
  const Camera& cam = *context.frame.camera;
  const QMatrix4x4& view_proj = context.frame.view_proj;

  struct GrassView {
    Buffer* instance_buffer;
    std::size_t instance_count;
    const GrassBatchParams& params;
  };
  const GrassView grass{
      deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.grass};
  if ((grass.instance_buffer == nullptr) || grass.instance_count == 0 ||
      (m_grass_shader == nullptr) || !m_grass_vao || m_grass_vertex_count == 0) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  if (context.state.bound_shader != m_grass_shader) {
    m_grass_shader->use();
    context.state.bound_shader = m_grass_shader;
    context.state.bound_texture = nullptr;
  }
  bind_visibility_mask(*m_grass_shader, deco_cmd_.visibility);

  if (m_grass_uniforms.view_proj != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(m_grass_uniforms.view_proj, view_proj);
  }
  if (m_grass_uniforms.time != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(m_grass_uniforms.time, grass.params.time);
  }
  if (m_grass_uniforms.wind_strength != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(m_grass_uniforms.wind_strength,
                                grass.params.wind_strength);
  }
  if (m_grass_uniforms.wind_speed != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(m_grass_uniforms.wind_speed, grass.params.wind_speed);
  }
  if (m_grass_uniforms.soil_color != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(m_grass_uniforms.soil_color, grass.params.soil_color);
  }
  if (m_grass_uniforms.light_dir != Shader::InvalidUniform) {
    QVector3D light_dir = grass.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    m_grass_shader->set_uniform(m_grass_uniforms.light_dir, light_dir);
  }

  if (m_grass_uniforms.viewport_size != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(
        m_grass_uniforms.viewport_size,
        QVector2D(static_cast<float>(std::max(context.frame.viewport_width, 1)),
                  static_cast<float>(std::max(context.frame.viewport_height, 1))));
  }
  if (m_grass_uniforms.camera_pos != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(m_grass_uniforms.camera_pos, cam.get_position());
  }
  if (m_grass_uniforms.ambient_boost != Shader::InvalidUniform) {
    m_grass_shader->set_uniform(m_grass_uniforms.ambient_boost,
                                grass.params.ambient_boost);
  }

  {
    const auto& grass_uniforms = m_grass_uniforms;
    const auto& cover = context.frame.surface.ground_cover;
    const bool cover_ready = cover.texture != nullptr;
    if (grass_uniforms.has_cover_texture != Shader::InvalidUniform) {
      m_grass_shader->set_uniform(grass_uniforms.has_cover_texture,
                                  cover_ready ? 1 : 0);
    }
    if (cover_ready) {
      cover.texture->bind(TextureUnit::terrain_cover);
      glActiveTexture(GL_TEXTURE0);
      if (grass_uniforms.cover_texture != Shader::InvalidUniform) {
        m_grass_shader->set_uniform(grass_uniforms.cover_texture,
                                    TextureUnit::terrain_cover);
      }
      if (grass_uniforms.cover_uv_scale != Shader::InvalidUniform) {
        m_grass_shader->set_uniform(grass_uniforms.cover_uv_scale, cover.uv_scale);
      }
      if (grass_uniforms.cover_uv_offset != Shader::InvalidUniform) {
        m_grass_shader->set_uniform(grass_uniforms.cover_uv_offset, cover.uv_offset);
      }
    }
  }

  glBindVertexArray(m_grass_vao.id());
  grass.instance_buffer->bind();
  const auto stride = static_cast<GLsizei>(sizeof(GrassInstanceGpu));
  apply_vertex_attrib_layout({{tex_coord,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(GrassInstanceGpu, pos_height)},
                              {instance_position,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(GrassInstanceGpu, color_width)},
                              {instance_scale,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(GrassInstanceGpu, sway_params)}});
  grass.instance_buffer->unbind();

  glDrawArraysInstanced(GL_TRIANGLES,
                        0,
                        m_grass_vertex_count,
                        static_cast<GLsizei>(grass.instance_count));
  tally_draw(static_cast<std::size_t>(m_grass_vertex_count), grass.instance_count);
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
