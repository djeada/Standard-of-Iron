#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

void Backend::execute_terrain_commands(const PreparedBatch& prepared,
                                       CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool const& polygon_offset_enabled = context.polygon_offset_enabled;
  const bool rigged_instancing_enabled = context.rigged_instancing_enabled;
  std::size_t const& debug_rigged_batches = context.debug_rigged_batches;
  std::size_t const& debug_rigged_cmds = context.debug_rigged_cmds;
  std::size_t const& debug_rigged_instanced_attempts =
      context.debug_rigged_instanced_attempts;
  std::size_t const& debug_rigged_instanced_successes =
      context.debug_rigged_instanced_successes;
  std::size_t const& debug_rigged_instanced_failures =
      context.debug_rigged_instanced_failures;
  std::size_t const& debug_rigged_single_draws = context.debug_rigged_single_draws;
  (void)cam;
  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;
  (void)rigged_instancing_enabled;
  (void)debug_rigged_batches;
  (void)debug_rigged_cmds;
  (void)debug_rigged_instanced_attempts;
  (void)debug_rigged_instanced_successes;
  (void)debug_rigged_instanced_failures;
  (void)debug_rigged_single_draws;

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

    QVector3D const fog_color(
        m_clear_color[red], m_clear_color[green], m_clear_color[blue]);
    QVector3D const camera_position = cam.get_position();
    float const fog_start = std::max(cam.get_near() + 5.0F, cam.get_far() * 0.18F);
    float const fog_end = std::max(fog_start + 1.0F, cam.get_far() * 0.62F);

    auto draw_surface = [&](const TerrainSurfaceCmd& single) {
      const QMatrix4x4 mvp = view_proj * single.model;
      auto const set_fog_uniforms = [&](const auto& uniforms) {
        if (uniforms.camera_position != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms.camera_position, camera_position);
        }
        if (uniforms.fog_color != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms.fog_color, fog_color);
        }
        if (uniforms.fog_start != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms.fog_start, fog_start);
        }
        if (uniforms.fog_end != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms.fog_end, fog_end);
        }
      };

      if (single.params.is_ground_plane) {
        if (m_terrain_pipeline->m_ground_uniforms.mvp != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.mvp, mvp);
        }
        if (m_terrain_pipeline->m_ground_uniforms.model != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.model,
                                     single.model);
        }
        if (m_terrain_pipeline->m_ground_uniforms.grass_primary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.grass_primary,
              single.params.grass_primary);
        }
        if (m_terrain_pipeline->m_ground_uniforms.grass_secondary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.grass_secondary,
              single.params.grass_secondary);
        }
        if (m_terrain_pipeline->m_ground_uniforms.grass_dry != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.grass_dry,
                                     single.params.grass_dry);
        }
        if (m_terrain_pipeline->m_ground_uniforms.soil_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.soil_color,
                                     single.params.soil_color);
        }
        if (m_terrain_pipeline->m_ground_uniforms.rock_low != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.rock_low,
                                     single.params.rock_low);
        }
        if (m_terrain_pipeline->m_ground_uniforms.rock_high != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.rock_high,
                                     single.params.rock_high);
        }
        if (m_terrain_pipeline->m_ground_uniforms.tint != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.tint,
                                     single.params.tint);
        }
        if (m_terrain_pipeline->m_ground_uniforms.noise_offset !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.noise_offset,
                                     single.params.noise_offset);
        }
        if (m_terrain_pipeline->m_ground_uniforms.noise_angle !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.noise_angle,
                                     single.params.noise_angle);
        }
        if (m_terrain_pipeline->m_ground_uniforms.tile_size != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.tile_size,
                                     single.params.tile_size);
        }
        if (m_terrain_pipeline->m_ground_uniforms.macro_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.macro_noise_scale,
              single.params.macro_noise_scale);
        }
        if (m_terrain_pipeline->m_ground_uniforms.detail_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.detail_noise_scale,
              single.params.detail_noise_scale);
        }
        if (m_terrain_pipeline->m_ground_uniforms.soil_blend_height !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.soil_blend_height,
              single.params.soil_blend_height);
        }
        if (m_terrain_pipeline->m_ground_uniforms.soil_blend_sharpness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.soil_blend_sharpness,
              single.params.soil_blend_sharpness);
        }
        if (m_terrain_pipeline->m_ground_uniforms.height_noise_strength !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.height_noise_strength,
              single.params.height_noise_strength);
        }
        if (m_terrain_pipeline->m_ground_uniforms.height_noise_frequency !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.height_noise_frequency,
              single.params.height_noise_frequency);
        }
        if (m_terrain_pipeline->m_ground_uniforms.ambient_boost !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.ambient_boost,
              single.params.ambient_boost);
        }
        if (m_terrain_pipeline->m_ground_uniforms.light_dir != Shader::InvalidUniform) {
          QVector3D light_dir = single.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.light_dir,
                                     light_dir);
        }
        if (m_terrain_pipeline->m_ground_uniforms.snow_coverage !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.snow_coverage,
              single.params.snow_coverage);
        }
        if (m_terrain_pipeline->m_ground_uniforms.moisture_level !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.moisture_level,
              single.params.moisture_level);
        }
        if (m_terrain_pipeline->m_ground_uniforms.crack_intensity !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.crack_intensity,
              single.params.crack_intensity);
        }
        if (m_terrain_pipeline->m_ground_uniforms.rock_exposure !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.rock_exposure,
              single.params.rock_exposure);
        }
        if (m_terrain_pipeline->m_ground_uniforms.grass_saturation !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.grass_saturation,
              single.params.grass_saturation);
        }
        if (m_terrain_pipeline->m_ground_uniforms.soil_roughness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.soil_roughness,
              single.params.soil_roughness);
        }
        if (m_terrain_pipeline->m_ground_uniforms.micro_bump_amp !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.micro_bump_amp,
              single.params.micro_bump_amp);
        }
        if (m_terrain_pipeline->m_ground_uniforms.micro_bump_freq !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.micro_bump_freq,
              single.params.micro_bump_freq);
        }
        if (m_terrain_pipeline->m_ground_uniforms.micro_normal_weight !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.micro_normal_weight,
              single.params.micro_normal_weight);
        }
        if (m_terrain_pipeline->m_ground_uniforms.albedo_jitter !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_ground_uniforms.albedo_jitter,
              single.params.albedo_jitter);
        }
        if (m_terrain_pipeline->m_ground_uniforms.snow_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.snow_color,
                                     single.params.snow_color);
        }
        set_fog_uniforms(m_terrain_pipeline->m_ground_uniforms);
      } else {
        const auto& visibility = single.visibility;
        if (m_terrain_pipeline->m_terrain_uniforms.has_visibility !=
            Shader::InvalidUniform) {
          int const has_vis =
              visibility.enabled && (visibility.texture != nullptr) ? 1 : 0;
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.has_visibility, has_vis);
        }
        if (visibility.enabled && visibility.texture != nullptr) {
          if (m_terrain_pipeline->m_terrain_uniforms.visibility_size !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.visibility_size,
                visibility.size);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.visibility_tile_size !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.visibility_tile_size,
                visibility.tile_size);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.explored_alpha !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.explored_alpha,
                visibility.explored_alpha);
          }
          constexpr int k_terrain_vis_texture_unit = 7;
          visibility.texture->bind(k_terrain_vis_texture_unit);
          m_last_bound_texture = visibility.texture;
          if (m_terrain_pipeline->m_terrain_uniforms.visibility_texture !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.visibility_texture,
                k_terrain_vis_texture_unit);
          }
        }
        if (m_terrain_pipeline->m_terrain_uniforms.mvp != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.mvp, mvp);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.model != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.model,
                                     single.model);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.grass_primary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.grass_primary,
              single.params.grass_primary);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.grass_secondary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.grass_secondary,
              single.params.grass_secondary);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.grass_dry !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.grass_dry,
                                     single.params.grass_dry);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.soil_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.soil_color,
                                     single.params.soil_color);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.rock_low != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.rock_low,
                                     single.params.rock_low);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.rock_high !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.rock_high,
                                     single.params.rock_high);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.tint != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.tint,
                                     single.params.tint);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.noise_offset !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.noise_offset,
              single.params.noise_offset);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.tile_size !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.tile_size,
                                     single.params.tile_size);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.macro_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.macro_noise_scale,
              single.params.macro_noise_scale);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.detail_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.detail_noise_scale,
              single.params.detail_noise_scale);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.slope_rock_threshold !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.slope_rock_threshold,
              single.params.slope_rock_threshold);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.slope_rock_sharpness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.slope_rock_sharpness,
              single.params.slope_rock_sharpness);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.soil_blend_height !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.soil_blend_height,
              single.params.soil_blend_height);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.soil_blend_sharpness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.soil_blend_sharpness,
              single.params.soil_blend_sharpness);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.height_noise_strength !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.height_noise_strength,
              single.params.height_noise_strength);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.height_noise_frequency !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.height_noise_frequency,
              single.params.height_noise_frequency);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.ambient_boost !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.ambient_boost,
              single.params.ambient_boost);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.rock_detail_strength !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.rock_detail_strength,
              single.params.rock_detail_strength);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.light_dir !=
            Shader::InvalidUniform) {
          QVector3D light_dir = single.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.light_dir,
                                     light_dir);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.snow_coverage !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.snow_coverage,
              single.params.snow_coverage);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.moisture_level !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.moisture_level,
              single.params.moisture_level);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.crack_intensity !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.crack_intensity,
              single.params.crack_intensity);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.rock_exposure !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.rock_exposure,
              single.params.rock_exposure);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.grass_saturation !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.grass_saturation,
              single.params.grass_saturation);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.soil_roughness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.soil_roughness,
              single.params.soil_roughness);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.curvature_response !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.curvature_response,
              single.params.curvature_response);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.ridge_response !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.ridge_response,
              single.params.ridge_response);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.gully_response !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.gully_response,
              single.params.gully_response);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.snow_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.snow_color,
                                     single.params.snow_color);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.soil_foot_height !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.soil_foot_height,
              single.params.soil_foot_height);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.screen_toe_mul !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.screen_toe_mul,
              single.params.screen_toe_mul);
        }
        if (m_terrain_pipeline->m_terrain_uniforms.screen_toe_clamp !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrain_pipeline->m_terrain_uniforms.screen_toe_clamp,
              single.params.screen_toe_clamp);
        }
        set_fog_uniforms(m_terrain_pipeline->m_terrain_uniforms);
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
