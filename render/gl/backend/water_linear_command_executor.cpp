#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

void Backend::execute_water_linear_commands(const PreparedBatch& prepared,
                                            CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool& polygon_offset_enabled = context.polygon_offset_enabled;
  const bool rigged_instancing_enabled = context.rigged_instancing_enabled;
  (void)cam;
  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;
  (void)rigged_instancing_enabled;

  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();
  const auto& cmd = queue.get_sorted(i);
  switch (cmd.index()) {
  case TerrainFeatureCmdIndex: {
    const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
    if (feature.mesh == nullptr || m_water_pipeline == nullptr) {
      break;
    }

    if (polygon_offset_enabled) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      polygon_offset_enabled = false;
    }

    const bool is_transparent = feature.alpha < 0.999F;
    std::optional<DepthMaskScope> transparent_depth_scope;
    std::optional<BlendScope> transparent_blend_scope;
    if (is_transparent) {
      transparent_depth_scope.emplace(false);
      transparent_blend_scope.emplace(true);
      glDepthFunc(GL_LEQUAL);
    } else {
      glDepthMask(GL_TRUE);
    }

    auto draw_feature_model = [&](Shader* shader,
                                  GLint model_uniform,
                                  GLint mvp_uniform,
                                  GLint color_uniform,
                                  GLint alpha_uniform = Shader::InvalidUniform,
                                  GLint road_surface_uniform = Shader::InvalidUniform) {
      if (shader == nullptr) {
        return;
      }
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& single = std::get<TerrainFeatureCmdIndex>(queue.get_sorted(j));
        if (mvp_uniform != Shader::InvalidUniform) {
          shader->set_uniform(mvp_uniform, view_proj * single.model);
        }
        if (model_uniform != Shader::InvalidUniform) {
          shader->set_uniform(model_uniform, single.model);
        }
        if (color_uniform != Shader::InvalidUniform) {
          shader->set_uniform(color_uniform, single.color);
        }
        if (alpha_uniform != Shader::InvalidUniform) {
          shader->set_uniform(alpha_uniform, single.alpha);
        }
        if (road_surface_uniform != Shader::InvalidUniform) {
          shader->set_uniform(road_surface_uniform,
                              static_cast<int>(single.road_surface_kind));
        }
        single.mesh->draw();
      }
    };

    auto set_water_environment = [&](Shader* shader, const auto& uniforms) {
      QVector3D const fog_color(
          m_clear_color[red], m_clear_color[green], m_clear_color[blue]);
      float const fog_start = std::max(cam.get_near() + 5.0F, cam.get_far() * 0.18F);
      float const fog_end = std::max(fog_start + 1.0F, cam.get_far() * 0.62F);
      if (uniforms.camera_position != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.camera_position, cam.get_position());
      }
      if (uniforms.light_direction != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.light_direction, m_light_dir);
      }
      if (uniforms.fog_color != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.fog_color, fog_color);
      }
      if (uniforms.fog_start != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.fog_start, fog_start);
      }
      if (uniforms.fog_end != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.fog_end, fog_end);
      }
    };

    switch (feature.kind) {
    case LinearFeatureKind::Water: {
      Shader* water_shader = m_water_pipeline->m_water_shader;
      if (water_shader == nullptr) {
        break;
      }
      const auto& visibility = feature.visibility;
      if (m_last_bound_shader != water_shader) {
        water_shader->use();
        water_shader->set_uniform(m_water_pipeline->m_water_uniforms.view, view);
        water_shader->set_uniform(m_water_pipeline->m_water_uniforms.projection,
                                  projection);
        water_shader->set_uniform(m_water_pipeline->m_water_uniforms.time,
                                  m_animation_time);
        set_water_environment(water_shader, m_water_pipeline->m_water_uniforms);
        m_last_bound_shader = water_shader;
        m_last_bound_texture = nullptr;
      }
      if (m_water_pipeline->m_water_uniforms.has_visibility != Shader::InvalidUniform) {
        int const has_vis =
            visibility.enabled && (visibility.texture != nullptr) ? 1 : 0;
        water_shader->set_uniform(m_water_pipeline->m_water_uniforms.has_visibility,
                                  has_vis);
      }
      if (visibility.enabled && visibility.texture != nullptr) {
        if (m_water_pipeline->m_water_uniforms.visibility_size !=
            Shader::InvalidUniform) {
          water_shader->set_uniform(m_water_pipeline->m_water_uniforms.visibility_size,
                                    visibility.size);
        }
        if (m_water_pipeline->m_water_uniforms.visibility_tile_size !=
            Shader::InvalidUniform) {
          water_shader->set_uniform(
              m_water_pipeline->m_water_uniforms.visibility_tile_size,
              visibility.tile_size);
        }
        if (m_water_pipeline->m_water_uniforms.explored_alpha !=
            Shader::InvalidUniform) {
          water_shader->set_uniform(m_water_pipeline->m_water_uniforms.explored_alpha,
                                    visibility.explored_alpha);
        }
        constexpr int k_water_vis_texture_unit = 7;
        visibility.texture->bind(k_water_vis_texture_unit);
        m_last_bound_texture = visibility.texture;
        if (m_water_pipeline->m_water_uniforms.visibility_texture !=
            Shader::InvalidUniform) {
          water_shader->set_uniform(
              m_water_pipeline->m_water_uniforms.visibility_texture,
              k_water_vis_texture_unit);
        }
      }
      {
        PolygonOffsetScope const poly(-1.0F, -1.0F);
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto& single = std::get<TerrainFeatureCmdIndex>(queue.get_sorted(j));
          if (m_water_pipeline->m_water_uniforms.surface_kind !=
              Shader::InvalidUniform) {
            water_shader->set_uniform(m_water_pipeline->m_water_uniforms.surface_kind,
                                      static_cast<int>(single.water_kind));
          }
          water_shader->set_uniform(m_water_pipeline->m_water_uniforms.model,
                                    single.model);
          if (m_water_pipeline->m_water_uniforms.segment_visibility !=
              Shader::InvalidUniform) {
            water_shader->set_uniform(
                m_water_pipeline->m_water_uniforms.segment_visibility, single.alpha);
          }
          single.mesh->draw();
        }
      }
      break;
    }
    case LinearFeatureKind::Shoreline: {
      Shader* riverbank_shader = m_water_pipeline->m_riverbank_shader;
      if (riverbank_shader == nullptr) {
        break;
      }
      const auto& visibility = feature.visibility;
      if (m_last_bound_shader != riverbank_shader) {
        riverbank_shader->use();
        riverbank_shader->set_uniform(m_water_pipeline->m_riverbank_uniforms.view,
                                      view);
        riverbank_shader->set_uniform(m_water_pipeline->m_riverbank_uniforms.projection,
                                      projection);
        riverbank_shader->set_uniform(m_water_pipeline->m_riverbank_uniforms.time,
                                      m_animation_time);
        set_water_environment(riverbank_shader, m_water_pipeline->m_riverbank_uniforms);
        m_last_bound_shader = riverbank_shader;
      }
      if (m_water_pipeline->m_riverbank_uniforms.has_visibility !=
          Shader::InvalidUniform) {
        int const has_vis =
            visibility.enabled && (visibility.texture != nullptr) ? 1 : 0;
        riverbank_shader->set_uniform(
            m_water_pipeline->m_riverbank_uniforms.has_visibility, has_vis);
      }
      if (visibility.enabled && visibility.texture != nullptr) {
        if (m_water_pipeline->m_riverbank_uniforms.visibility_size !=
            Shader::InvalidUniform) {
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.visibility_size, visibility.size);
        }
        if (m_water_pipeline->m_riverbank_uniforms.visibility_tile_size !=
            Shader::InvalidUniform) {
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.visibility_tile_size,
              visibility.tile_size);
        }
        if (m_water_pipeline->m_riverbank_uniforms.explored_alpha !=
            Shader::InvalidUniform) {
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.explored_alpha,
              visibility.explored_alpha);
        }
        constexpr int k_riverbank_vis_texture_unit = 7;
        visibility.texture->bind(k_riverbank_vis_texture_unit);
        if (m_water_pipeline->m_riverbank_uniforms.visibility_texture !=
            Shader::InvalidUniform) {
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.visibility_texture,
              k_riverbank_vis_texture_unit);
        }
        m_last_bound_texture = visibility.texture;
      }
      {
        PolygonOffsetScope const poly(-1.0F, -1.0F);
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto& single = std::get<TerrainFeatureCmdIndex>(queue.get_sorted(j));
          if (m_water_pipeline->m_riverbank_uniforms.surface_kind !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.surface_kind,
                static_cast<int>(single.water_kind));
          }
          if (m_water_pipeline->m_riverbank_uniforms.ground_color !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.ground_color, single.color);
          }
          if (m_water_pipeline->m_riverbank_uniforms.grass_secondary !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.grass_secondary,
                single.biome_grass_secondary);
          }
          if (m_water_pipeline->m_riverbank_uniforms.grass_dry !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.grass_dry,
                single.biome_grass_dry);
          }
          if (m_water_pipeline->m_riverbank_uniforms.soil_color !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.soil_color,
                single.biome_soil_color);
          }
          if (m_water_pipeline->m_riverbank_uniforms.rock_low !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.rock_low, single.biome_rock_low);
          }
          if (m_water_pipeline->m_riverbank_uniforms.rock_high !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.rock_high,
                single.biome_rock_high);
          }
          if (m_water_pipeline->m_riverbank_uniforms.snow_color !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.snow_color,
                single.biome_snow_color);
          }
          if (m_water_pipeline->m_riverbank_uniforms.moisture !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.moisture, single.biome_moisture);
          }
          if (m_water_pipeline->m_riverbank_uniforms.rock_exposure !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.rock_exposure,
                single.biome_rock_exposure);
          }
          if (m_water_pipeline->m_riverbank_uniforms.snow_coverage !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.snow_coverage,
                single.biome_snow_coverage);
          }
          riverbank_shader->set_uniform(m_water_pipeline->m_riverbank_uniforms.model,
                                        single.model);
          if (m_water_pipeline->m_riverbank_uniforms.segment_visibility !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.segment_visibility,
                single.alpha);
          }
          single.mesh->draw();
        }
      }
      break;
    }
    case LinearFeatureKind::Bridge: {
      Shader* bridge_shader = m_water_pipeline->m_bridge_shader;
      if (bridge_shader == nullptr) {
        break;
      }
      if (m_last_bound_shader != bridge_shader) {
        bridge_shader->use();
        bridge_shader->set_uniform(m_water_pipeline->m_bridge_uniforms.light_direction,
                                   m_light_dir);
        m_last_bound_shader = bridge_shader;
        m_last_bound_texture = nullptr;
      }
      draw_feature_model(bridge_shader,
                         m_water_pipeline->m_bridge_uniforms.model,
                         m_water_pipeline->m_bridge_uniforms.mvp,
                         m_water_pipeline->m_bridge_uniforms.color);
      break;
    }
    case LinearFeatureKind::Road: {
      Shader* road_shader = m_water_pipeline->m_road_shader;
      if (road_shader == nullptr) {
        break;
      }
      const auto& visibility = feature.visibility;
      if (m_last_bound_shader != road_shader) {
        road_shader->use();
        road_shader->set_uniform(m_water_pipeline->m_road_uniforms.light_direction,
                                 m_light_dir);
        m_last_bound_shader = road_shader;
        m_last_bound_texture = nullptr;
      }
      if (m_water_pipeline->m_road_uniforms.has_visibility != Shader::InvalidUniform) {
        int const has_vis =
            visibility.enabled && (visibility.texture != nullptr) ? 1 : 0;
        road_shader->set_uniform(m_water_pipeline->m_road_uniforms.has_visibility,
                                 has_vis);
      }
      if (visibility.enabled && visibility.texture != nullptr) {
        if (m_water_pipeline->m_road_uniforms.visibility_size !=
            Shader::InvalidUniform) {
          road_shader->set_uniform(m_water_pipeline->m_road_uniforms.visibility_size,
                                   visibility.size);
        }
        if (m_water_pipeline->m_road_uniforms.visibility_tile_size !=
            Shader::InvalidUniform) {
          road_shader->set_uniform(
              m_water_pipeline->m_road_uniforms.visibility_tile_size,
              visibility.tile_size);
        }
        if (m_water_pipeline->m_road_uniforms.explored_alpha !=
            Shader::InvalidUniform) {
          road_shader->set_uniform(m_water_pipeline->m_road_uniforms.explored_alpha,
                                   visibility.explored_alpha);
        }
        constexpr int k_road_vis_texture_unit = 7;
        visibility.texture->bind(k_road_vis_texture_unit);
        m_last_bound_texture = visibility.texture;
        if (m_water_pipeline->m_road_uniforms.visibility_texture !=
            Shader::InvalidUniform) {
          road_shader->set_uniform(m_water_pipeline->m_road_uniforms.visibility_texture,
                                   k_road_vis_texture_unit);
        }
      }
      {
        PolygonOffsetScope const poly(-1.0F, -1.0F);
        draw_feature_model(road_shader,
                           m_water_pipeline->m_road_uniforms.model,
                           m_water_pipeline->m_road_uniforms.mvp,
                           m_water_pipeline->m_road_uniforms.color,
                           m_water_pipeline->m_road_uniforms.alpha,
                           m_water_pipeline->m_road_uniforms.surface_kind);
      }
      break;
    }
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
