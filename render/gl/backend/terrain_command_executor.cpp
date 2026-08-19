#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
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

void Backend::set_ground_plane_uniforms(Shader& shader,
                                        const TerrainSurfaceCmd& single,
                                        const QMatrix4x4& mvp,
                                        const QVector3D& camera_position) {
  const auto& pipeline = *m_terrain_pipeline;
  if (pipeline.m_ground_uniforms.mvp != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.mvp, mvp);
  }
  if (pipeline.m_ground_uniforms.model != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.model, single.model);
  }
  if (pipeline.m_ground_uniforms.ground_type != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.ground_type,
                       single.params.ground_type);
  }
  if (pipeline.m_ground_uniforms.grass_primary != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.grass_primary,
                       single.params.grass_primary);
  }
  if (pipeline.m_ground_uniforms.grass_secondary != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.grass_secondary,
                       single.params.grass_secondary);
  }
  if (pipeline.m_ground_uniforms.grass_dry != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.grass_dry, single.params.grass_dry);
  }
  if (pipeline.m_ground_uniforms.soil_color != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.soil_color, single.params.soil_color);
  }
  if (pipeline.m_ground_uniforms.rock_low != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.rock_low, single.params.rock_low);
  }
  if (pipeline.m_ground_uniforms.rock_high != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.rock_high, single.params.rock_high);
  }
  if (pipeline.m_ground_uniforms.tint != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.tint, single.params.tint);
  }
  if (pipeline.m_ground_uniforms.noise_offset != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.noise_offset,
                       single.params.noise_offset);
  }
  if (pipeline.m_ground_uniforms.noise_angle != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.noise_angle,
                       single.params.noise_angle);
  }
  if (pipeline.m_ground_uniforms.tile_size != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.tile_size, single.params.tile_size);
  }
  if (pipeline.m_ground_uniforms.macro_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.macro_noise_scale,
                       single.params.macro_noise_scale);
  }
  if (pipeline.m_ground_uniforms.detail_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.detail_noise_scale,
                       single.params.detail_noise_scale);
  }
  if (pipeline.m_ground_uniforms.soil_blend_height != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.soil_blend_height,
                       single.params.soil_blend_height);
  }
  if (pipeline.m_ground_uniforms.soil_blend_sharpness != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.soil_blend_sharpness,
                       single.params.soil_blend_sharpness);
  }
  if (pipeline.m_ground_uniforms.height_noise_strength != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.height_noise_strength,
                       single.params.height_noise_strength);
  }
  if (pipeline.m_ground_uniforms.height_noise_frequency != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.height_noise_frequency,
                       single.params.height_noise_frequency);
  }
  if (pipeline.m_ground_uniforms.ambient_boost != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.ambient_boost,
                       single.params.ambient_boost);
  }
  if (pipeline.m_ground_uniforms.light_dir != Shader::InvalidUniform) {
    QVector3D light_dir = single.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    shader.set_uniform(pipeline.m_ground_uniforms.light_dir, light_dir);
  }
  if (pipeline.m_ground_uniforms.snow_coverage != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.snow_coverage,
                       single.params.snow_coverage);
  }
  if (pipeline.m_ground_uniforms.moisture_level != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.moisture_level,
                       single.params.moisture_level);
  }
  if (pipeline.m_ground_uniforms.crack_intensity != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.crack_intensity,
                       single.params.crack_intensity);
  }
  if (pipeline.m_ground_uniforms.rock_exposure != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.rock_exposure,
                       single.params.rock_exposure);
  }
  if (pipeline.m_ground_uniforms.grass_saturation != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.grass_saturation,
                       single.params.grass_saturation);
  }
  if (pipeline.m_ground_uniforms.soil_roughness != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.soil_roughness,
                       single.params.soil_roughness);
  }
  if (pipeline.m_ground_uniforms.micro_bump_amp != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.micro_bump_amp,
                       single.params.micro_bump_amp);
  }
  if (pipeline.m_ground_uniforms.micro_bump_freq != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.micro_bump_freq,
                       single.params.micro_bump_freq);
  }
  if (pipeline.m_ground_uniforms.micro_normal_weight != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.micro_normal_weight,
                       single.params.micro_normal_weight);
  }
  if (pipeline.m_ground_uniforms.albedo_jitter != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.albedo_jitter,
                       single.params.albedo_jitter);
  }
  if (pipeline.m_ground_uniforms.snow_color != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_ground_uniforms.snow_color, single.params.snow_color);
  }
  set_camera_uniform(shader, pipeline.m_ground_uniforms, camera_position);
}

void Backend::set_terrain_chunk_uniforms(Shader& shader,
                                         const TerrainSurfaceCmd& single,
                                         const QMatrix4x4& mvp,
                                         const QVector3D& camera_position) {
  const auto& pipeline = *m_terrain_pipeline;
  const auto& visibility = single.visibility;
  if (pipeline.m_terrain_uniforms.has_visibility != Shader::InvalidUniform) {
    int const has_vis = visibility.enabled && (visibility.texture != nullptr) ? 1 : 0;
    shader.set_uniform(pipeline.m_terrain_uniforms.has_visibility, has_vis);
  }
  if (visibility.enabled && visibility.texture != nullptr) {
    if (pipeline.m_terrain_uniforms.visibility_size != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.visibility_size, visibility.size);
    }
    if (pipeline.m_terrain_uniforms.visibility_tile_size != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.visibility_tile_size,
                         visibility.tile_size);
    }
    if (pipeline.m_terrain_uniforms.explored_alpha != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.explored_alpha,
                         visibility.explored_alpha);
    }
    visibility.texture->bind(TextureUnit::terrain_visibility);
    m_last_bound_texture = visibility.texture;
    if (pipeline.m_terrain_uniforms.visibility_texture != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.visibility_texture,
                         TextureUnit::terrain_visibility);
    }
  }
  if (pipeline.m_terrain_uniforms.mvp != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.mvp, mvp);
  }
  if (pipeline.m_terrain_uniforms.model != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.model, single.model);
  }
  if (pipeline.m_terrain_uniforms.ground_type != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.ground_type,
                       single.params.ground_type);
  }
  if (pipeline.m_terrain_uniforms.terrain_type != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.terrain_type,
                       single.params.terrain_type);
  }
  if (pipeline.m_terrain_uniforms.grass_primary != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.grass_primary,
                       single.params.grass_primary);
  }
  if (pipeline.m_terrain_uniforms.grass_secondary != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.grass_secondary,
                       single.params.grass_secondary);
  }
  if (pipeline.m_terrain_uniforms.grass_dry != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.grass_dry, single.params.grass_dry);
  }
  if (pipeline.m_terrain_uniforms.soil_color != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.soil_color,
                       single.params.soil_color);
  }
  if (pipeline.m_terrain_uniforms.rock_low != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.rock_low, single.params.rock_low);
  }
  if (pipeline.m_terrain_uniforms.rock_high != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.rock_high, single.params.rock_high);
  }
  if (pipeline.m_terrain_uniforms.tint != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.tint, single.params.tint);
  }
  if (pipeline.m_terrain_uniforms.noise_offset != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.noise_offset,
                       single.params.noise_offset);
  }
  if (pipeline.m_terrain_uniforms.tile_size != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.tile_size, single.params.tile_size);
  }
  if (pipeline.m_terrain_uniforms.macro_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.macro_noise_scale,
                       single.params.macro_noise_scale);
  }
  if (pipeline.m_terrain_uniforms.detail_noise_scale != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.detail_noise_scale,
                       single.params.detail_noise_scale);
  }
  if (pipeline.m_terrain_uniforms.slope_rock_threshold != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.slope_rock_threshold,
                       single.params.slope_rock_threshold);
  }
  if (pipeline.m_terrain_uniforms.slope_rock_sharpness != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.slope_rock_sharpness,
                       single.params.slope_rock_sharpness);
  }
  if (pipeline.m_terrain_uniforms.soil_blend_height != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.soil_blend_height,
                       single.params.soil_blend_height);
  }
  if (pipeline.m_terrain_uniforms.soil_blend_sharpness != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.soil_blend_sharpness,
                       single.params.soil_blend_sharpness);
  }
  if (pipeline.m_terrain_uniforms.height_noise_strength != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.height_noise_strength,
                       single.params.height_noise_strength);
  }
  if (pipeline.m_terrain_uniforms.height_noise_frequency != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.height_noise_frequency,
                       single.params.height_noise_frequency);
  }
  if (pipeline.m_terrain_uniforms.ambient_boost != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.ambient_boost,
                       single.params.ambient_boost);
  }
  if (pipeline.m_terrain_uniforms.rock_detail_strength != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.rock_detail_strength,
                       single.params.rock_detail_strength);
  }
  if (pipeline.m_terrain_uniforms.light_dir != Shader::InvalidUniform) {
    QVector3D light_dir = single.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    shader.set_uniform(pipeline.m_terrain_uniforms.light_dir, light_dir);
  }
  if (pipeline.m_terrain_uniforms.snow_coverage != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.snow_coverage,
                       single.params.snow_coverage);
  }
  if (pipeline.m_terrain_uniforms.moisture_level != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.moisture_level,
                       single.params.moisture_level);
  }
  if (pipeline.m_terrain_uniforms.crack_intensity != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.crack_intensity,
                       single.params.crack_intensity);
  }
  if (pipeline.m_terrain_uniforms.rock_exposure != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.rock_exposure,
                       single.params.rock_exposure);
  }
  if (pipeline.m_terrain_uniforms.grass_saturation != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.grass_saturation,
                       single.params.grass_saturation);
  }
  if (pipeline.m_terrain_uniforms.soil_roughness != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.soil_roughness,
                       single.params.soil_roughness);
  }
  if (pipeline.m_terrain_uniforms.curvature_response != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.curvature_response,
                       single.params.curvature_response);
  }
  if (pipeline.m_terrain_uniforms.ridge_response != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.ridge_response,
                       single.params.ridge_response);
  }
  if (pipeline.m_terrain_uniforms.gully_response != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.gully_response,
                       single.params.gully_response);
  }
  if (pipeline.m_terrain_uniforms.snow_color != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.snow_color,
                       single.params.snow_color);
  }
  if (pipeline.m_terrain_uniforms.soil_foot_height != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.soil_foot_height,
                       single.params.soil_foot_height);
  }
  if (pipeline.m_terrain_uniforms.screen_toe_mul != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.screen_toe_mul,
                       single.params.screen_toe_mul);
  }
  if (pipeline.m_terrain_uniforms.screen_toe_clamp != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.screen_toe_clamp,
                       single.params.screen_toe_clamp);
  }
  const auto& height = single.height;
  if (pipeline.m_terrain_uniforms.has_height_texture != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.has_height_texture,
                       height.enabled && height.texture != nullptr ? 1 : 0);
  }
  if (height.enabled && height.texture != nullptr) {
    height.texture->bind(TextureUnit::terrain_height);
    m_last_bound_texture = height.texture;
    if (pipeline.m_terrain_uniforms.height_texture != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.height_texture,
                         TextureUnit::terrain_height);
    }
    if (pipeline.m_terrain_uniforms.height_texel_size != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.height_texel_size,
                         height.texel_size);
    }
    if (pipeline.m_terrain_uniforms.height_uv_scale != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.height_uv_scale, height.uv_scale);
    }
    if (pipeline.m_terrain_uniforms.height_uv_offset != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.height_uv_offset,
                         height.uv_offset);
    }
    if (pipeline.m_terrain_uniforms.height_to_world != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.height_to_world, height.to_world);
    }
  }
  const bool field_ready = height.enabled && height.field_texture != nullptr;
  if (pipeline.m_terrain_uniforms.has_field_texture != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.has_field_texture,
                       field_ready ? 1 : 0);
  }
  if (field_ready) {
    height.field_texture->bind(TextureUnit::terrain_fields);
    if (pipeline.m_terrain_uniforms.field_texture != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.field_texture,
                         TextureUnit::terrain_fields);
    }
  }

  const bool atlas_ready = height.noise_atlas != 0U;
  if (pipeline.m_terrain_uniforms.has_noise_atlas != Shader::InvalidUniform) {
    shader.set_uniform(pipeline.m_terrain_uniforms.has_noise_atlas,
                       atlas_ready ? 1 : 0);
  }
  if (atlas_ready) {
    glActiveTexture(GL_TEXTURE0 + TextureUnit::terrain_noise_atlas);
    glBindTexture(GL_TEXTURE_2D, height.noise_atlas);
    glActiveTexture(GL_TEXTURE0);
    if (pipeline.m_terrain_uniforms.noise_atlas != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.noise_atlas,
                         TextureUnit::terrain_noise_atlas);
    }
    if (pipeline.m_terrain_uniforms.noise_atlas_world_size != Shader::InvalidUniform) {
      shader.set_uniform(pipeline.m_terrain_uniforms.noise_atlas_world_size,
                         height.noise_atlas_world_size);
    }
  }
  set_camera_uniform(shader, pipeline.m_terrain_uniforms, camera_position);
}

void Backend::execute_terrain_commands(const PreparedBatch& prepared,
                                       CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool const& polygon_offset_enabled = context.polygon_offset_enabled;
  (void)cam;
  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;

  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();
  const auto& cmd = queue.get_sorted(i);
  switch (cmd.index()) {
  case TerrainSurfaceCmdIndex: {
    const auto& terrain = std::get<TerrainSurfaceCmdIndex>(cmd);

    Shader* active_shader = terrain.params.is_ground_plane
                                ? m_terrain_pipeline->m_ground_shader
                                : m_terrain_pipeline->m_terrain_shader;

    if ((terrain.mesh == nullptr) || (active_shader == nullptr)) {
      break;
    }

    if (m_last_bound_shader != active_shader) {
      active_shader->use();
      m_last_bound_shader = active_shader;
      m_last_bound_texture = nullptr;
    }

    QVector3D const camera_position = cam.get_position();

    auto draw_surface = [&](const TerrainSurfaceCmd& single) {
      const QMatrix4x4 mvp = view_proj * single.model;
      if (single.params.is_ground_plane) {
        set_ground_plane_uniforms(*active_shader, single, mvp, camera_position);
      } else {
        set_terrain_chunk_uniforms(*active_shader, single, mvp, camera_position);
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
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
