#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <variant>

#include "render/backend/state_scopes.h"
#include "render/backend/water_pipeline.h"
#include "render/frame/draw_commands.h"
#include "render/frame/draw_queue.h"
#include "render/geom/mesh.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"
#include "scene/camera.h"

namespace Render::GL::BackendPipelines {

namespace {

constexpr int k_visibility_texture_unit = 7;

template <typename Uniforms>
void set_water_environment(Shader* shader,
                           const Uniforms& uniforms,
                           const Render::Pass::FrameContext& frame) {
  if (uniforms.camera_position != Shader::InvalidUniform) {
    shader->set_uniform(uniforms.camera_position, frame.camera->get_position());
  }
  if (uniforms.light_direction != Shader::InvalidUniform) {
    shader->set_uniform(uniforms.light_direction, frame.light_direction);
  }
}

} // namespace

void WaterPipeline::draw_terrain_features(const PreparedBatch& prepared,
                                          PipelineDrawContext& context) {
  const auto& cmd = context.queue.get_sorted(prepared.start);
  const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
  if (feature.mesh == nullptr) {
    return;
  }

  bool& polygon_offset_enabled = context.state.polygon_offset_enabled;
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

  switch (feature.kind) {
  case LinearFeatureKind::Water:
    draw_water(prepared, context, feature);
    break;
  case LinearFeatureKind::Shoreline:
    draw_riverbank(prepared, context, feature, is_transparent);
    break;
  case LinearFeatureKind::Bridge:
    draw_bridge(prepared, context, feature);
    break;
  case LinearFeatureKind::Road:
    draw_road(prepared, context, feature);
    break;
  }
}

void WaterPipeline::draw_feature_models(const PreparedBatch& prepared,
                                        PipelineDrawContext& context,
                                        Shader* shader,
                                        Shader::UniformHandle model_uniform,
                                        Shader::UniformHandle mvp_uniform,
                                        Shader::UniformHandle color_uniform,
                                        Shader::UniformHandle alpha_uniform,
                                        Shader::UniformHandle road_surface_uniform) {
  if (shader == nullptr) {
    return;
  }
  const QMatrix4x4& view_proj = context.frame.view_proj;
  for (std::size_t j = prepared.start; j < prepared.end(); ++j) {
    const auto& single = std::get<TerrainFeatureCmdIndex>(context.queue.get_sorted(j));
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
      const auto& uniforms = m_road_uniforms;
      shader->set_uniform(uniforms.verge_grass, single.biome_grass_secondary);
      shader->set_uniform(uniforms.verge_soil, single.biome_soil_color);
      shader->set_uniform(uniforms.ground_type, single.biome_ground_type);
      shader->set_uniform(road_surface_uniform,
                          static_cast<int>(single.road_surface_kind));
    }
    context.meshes.draw(*single.mesh);
  }
}

void WaterPipeline::draw_water(const PreparedBatch& prepared,
                               PipelineDrawContext& context,
                               const TerrainFeatureCmd& feature) {
  Shader* water_shader = m_water_shader;
  if (water_shader == nullptr) {
    return;
  }
  const auto& uniforms = m_water_uniforms;
  auto& state = context.state;
  const auto& visibility = feature.visibility;
  Texture* const visibility_tex = context.textures.resolve(visibility.texture);
  if (state.bound_shader != water_shader) {
    water_shader->use();
    water_shader->set_uniform(uniforms.view, context.frame.view);
    water_shader->set_uniform(uniforms.projection, context.frame.projection);
    water_shader->set_uniform(uniforms.time, context.frame.animation_time);
    set_water_environment(water_shader, uniforms, context.frame);
    state.bound_shader = water_shader;
    state.bound_texture = nullptr;
  }
  if (uniforms.has_visibility != Shader::InvalidUniform) {
    int const has_vis = visibility.enabled && (visibility_tex != nullptr) ? 1 : 0;
    water_shader->set_uniform(uniforms.has_visibility, has_vis);
  }
  if (visibility.enabled && visibility_tex != nullptr) {
    if (uniforms.visibility_size != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.visibility_size, visibility.size);
    }
    if (uniforms.visibility_tile_size != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.visibility_tile_size, visibility.tile_size);
    }
    if (uniforms.explored_alpha != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.explored_alpha, visibility.explored_alpha);
    }
    visibility_tex->bind(k_visibility_texture_unit);
    state.bound_texture = visibility_tex;
    if (uniforms.visibility_texture != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.visibility_texture, k_visibility_texture_unit);
    }
  }
  PolygonOffsetScope const poly(-1.0F, -1.0F);
  for (std::size_t j = prepared.start; j < prepared.end(); ++j) {
    const auto& single = std::get<TerrainFeatureCmdIndex>(context.queue.get_sorted(j));
    if (uniforms.soil_color != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.soil_color, single.biome_soil_color);
    }
    if (uniforms.moisture != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.moisture, single.biome_moisture);
    }
    if (uniforms.snow_coverage != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.snow_coverage, single.biome_snow_coverage);
    }
    if (uniforms.surface_kind != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.surface_kind,
                                static_cast<int>(single.water_kind));
    }
    if (uniforms.calm != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.calm, single.water_calm);
    }
    if (uniforms.ford_count != Shader::InvalidUniform) {
      int const fords =
          std::clamp(single.ford_patch_count, 0, TerrainFeatureCmd::k_max_ford_patches);
      water_shader->set_uniform(uniforms.ford_count, fords);
      for (int patch = 0; patch < fords; ++patch) {
        auto const slot = static_cast<std::size_t>(patch);
        if (uniforms.ford_a[slot] != Shader::InvalidUniform) {
          water_shader->set_uniform(uniforms.ford_a[slot], single.ford_patch_a[slot]);
        }
        if (uniforms.ford_b[slot] != Shader::InvalidUniform) {
          water_shader->set_uniform(uniforms.ford_b[slot], single.ford_patch_b[slot]);
        }
      }
    }
    if (uniforms.neighbor_count != Shader::InvalidUniform) {
      int const neighbors = std::clamp(
          single.water_neighbor_count, 0, TerrainFeatureCmd::k_max_water_neighbors);
      water_shader->set_uniform(uniforms.neighbor_count, neighbors);
      for (int neighbor = 0; neighbor < neighbors; ++neighbor) {
        auto const slot = static_cast<std::size_t>(neighbor);
        if (uniforms.neighbor_segment[slot] != Shader::InvalidUniform) {
          water_shader->set_uniform(uniforms.neighbor_segment[slot],
                                    single.water_neighbor_segment[slot]);
        }
        if (uniforms.neighbor_radius[slot] != Shader::InvalidUniform) {
          water_shader->set_uniform(uniforms.neighbor_radius[slot],
                                    single.water_neighbor_radius[slot]);
        }
      }
    }
    water_shader->set_uniform(uniforms.model, single.model);
    if (uniforms.segment_visibility != Shader::InvalidUniform) {
      water_shader->set_uniform(uniforms.segment_visibility, single.alpha);
    }
    context.meshes.draw(*single.mesh);
  }
}

void WaterPipeline::draw_riverbank(const PreparedBatch& prepared,
                                   PipelineDrawContext& context,
                                   const TerrainFeatureCmd& feature,
                                   bool is_transparent) {
  Shader* riverbank_shader = m_riverbank_shader;
  if (riverbank_shader == nullptr) {
    return;
  }
  const auto& uniforms = m_riverbank_uniforms;
  auto& state = context.state;

  std::optional<BlendScope> shore_blend_scope;
  if (!is_transparent) {
    shore_blend_scope.emplace(true);
  }
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  const auto& visibility = feature.visibility;
  Texture* const visibility_tex = context.textures.resolve(visibility.texture);
  if (state.bound_shader != riverbank_shader) {
    riverbank_shader->use();
    riverbank_shader->set_uniform(uniforms.view, context.frame.view);
    riverbank_shader->set_uniform(uniforms.projection, context.frame.projection);
    riverbank_shader->set_uniform(uniforms.time, context.frame.animation_time);
    set_water_environment(riverbank_shader, uniforms, context.frame);
    state.bound_shader = riverbank_shader;
  }
  if (uniforms.has_visibility != Shader::InvalidUniform) {
    int const has_vis = visibility.enabled && (visibility_tex != nullptr) ? 1 : 0;
    riverbank_shader->set_uniform(uniforms.has_visibility, has_vis);
  }
  if (visibility.enabled && visibility_tex != nullptr) {
    if (uniforms.visibility_size != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.visibility_size, visibility.size);
    }
    if (uniforms.visibility_tile_size != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.visibility_tile_size,
                                    visibility.tile_size);
    }
    if (uniforms.explored_alpha != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.explored_alpha, visibility.explored_alpha);
    }
    visibility_tex->bind(k_visibility_texture_unit);
    if (uniforms.visibility_texture != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.visibility_texture,
                                    k_visibility_texture_unit);
    }
    state.bound_texture = visibility_tex;
  }
  PolygonOffsetScope const poly(-1.0F, -1.0F);
  for (std::size_t j = prepared.start; j < prepared.end(); ++j) {
    const auto& single = std::get<TerrainFeatureCmdIndex>(context.queue.get_sorted(j));
    if (uniforms.surface_kind != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.surface_kind,
                                    static_cast<int>(single.water_kind));
    }
    if (uniforms.ground_type != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.ground_type, single.biome_ground_type);
    }
    if (uniforms.ground_color != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.ground_color, single.color);
    }
    if (uniforms.grass_secondary != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.grass_secondary,
                                    single.biome_grass_secondary);
    }
    if (uniforms.grass_dry != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.grass_dry, single.biome_grass_dry);
    }
    if (uniforms.soil_color != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.soil_color, single.biome_soil_color);
    }
    if (uniforms.rock_low != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.rock_low, single.biome_rock_low);
    }
    if (uniforms.rock_high != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.rock_high, single.biome_rock_high);
    }
    if (uniforms.snow_color != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.snow_color, single.biome_snow_color);
    }
    if (uniforms.moisture != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.moisture, single.biome_moisture);
    }
    if (uniforms.rock_exposure != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.rock_exposure, single.biome_rock_exposure);
    }
    if (uniforms.snow_coverage != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.snow_coverage, single.biome_snow_coverage);
    }
    if (uniforms.ambient_boost != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.ambient_boost, single.ambient_boost);
    }
    riverbank_shader->set_uniform(uniforms.model, single.model);
    if (uniforms.segment_visibility != Shader::InvalidUniform) {
      riverbank_shader->set_uniform(uniforms.segment_visibility, single.alpha);
    }
    context.meshes.draw(*single.mesh);
  }
}

void WaterPipeline::draw_bridge(const PreparedBatch& prepared,
                                PipelineDrawContext& context,
                                const TerrainFeatureCmd& feature) {
  Shader* bridge_shader = m_bridge_shader;
  if (bridge_shader == nullptr) {
    return;
  }
  const auto& bridge_uniforms = m_bridge_uniforms;
  auto& state = context.state;
  const auto& fog_mask = feature.fog_mask;
  Texture* const fog_mask_tex = context.textures.resolve(fog_mask.texture);
  if (state.bound_shader != bridge_shader) {
    bridge_shader->use();
    set_water_environment(bridge_shader, bridge_uniforms, context.frame);
    state.bound_shader = bridge_shader;
    state.bound_texture = nullptr;
  }

  const bool has_fog_mask = fog_mask.enabled && (fog_mask_tex != nullptr);

  std::optional<BlendScope> reveal_blend_scope;
  if (has_fog_mask) {
    reveal_blend_scope.emplace(true);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
  }
  if (bridge_uniforms.has_fog_mask != Shader::InvalidUniform) {
    bridge_shader->set_uniform(bridge_uniforms.has_fog_mask, has_fog_mask ? 1 : 0);
  }
  if (has_fog_mask) {
    if (bridge_uniforms.fog_mask_size != Shader::InvalidUniform) {
      bridge_shader->set_uniform(bridge_uniforms.fog_mask_size, fog_mask.size);
    }
    if (bridge_uniforms.fog_mask_tile_size != Shader::InvalidUniform) {
      bridge_shader->set_uniform(bridge_uniforms.fog_mask_tile_size,
                                 fog_mask.tile_size);
    }
    if (bridge_uniforms.explored_alpha != Shader::InvalidUniform) {
      bridge_shader->set_uniform(bridge_uniforms.explored_alpha,
                                 feature.visibility.explored_alpha);
    }
    fog_mask_tex->bind(TextureUnit::terrain_visibility);
    state.bound_texture = fog_mask_tex;
    if (bridge_uniforms.fog_mask_texture != Shader::InvalidUniform) {
      bridge_shader->set_uniform(bridge_uniforms.fog_mask_texture,
                                 TextureUnit::terrain_visibility);
    }
  }

  draw_feature_models(prepared,
                      context,
                      bridge_shader,
                      bridge_uniforms.model,
                      bridge_uniforms.mvp,
                      bridge_uniforms.color);
}

void WaterPipeline::draw_road(const PreparedBatch& prepared,
                              PipelineDrawContext& context,
                              const TerrainFeatureCmd& feature) {
  Shader* road_shader = m_road_shader;
  if (road_shader == nullptr) {
    return;
  }
  const auto& road_uniforms = m_road_uniforms;
  auto& state = context.state;
  const auto& visibility = feature.visibility;
  Texture* const visibility_tex = context.textures.resolve(visibility.texture);
  if (state.bound_shader != road_shader) {
    road_shader->use();
    set_water_environment(road_shader, road_uniforms, context.frame);
    state.bound_shader = road_shader;
    state.bound_texture = nullptr;
  }
  if (road_uniforms.has_visibility != Shader::InvalidUniform) {
    int const has_vis = visibility.enabled && (visibility_tex != nullptr) ? 1 : 0;
    road_shader->set_uniform(road_uniforms.has_visibility, has_vis);
  }
  if (visibility.enabled && visibility_tex != nullptr) {
    if (road_uniforms.visibility_size != Shader::InvalidUniform) {
      road_shader->set_uniform(road_uniforms.visibility_size, visibility.size);
    }
    if (road_uniforms.visibility_tile_size != Shader::InvalidUniform) {
      road_shader->set_uniform(road_uniforms.visibility_tile_size,
                               visibility.tile_size);
    }
    if (road_uniforms.explored_alpha != Shader::InvalidUniform) {
      road_shader->set_uniform(road_uniforms.explored_alpha, visibility.explored_alpha);
    }
    visibility_tex->bind(k_visibility_texture_unit);
    state.bound_texture = visibility_tex;
    if (road_uniforms.visibility_texture != Shader::InvalidUniform) {
      road_shader->set_uniform(road_uniforms.visibility_texture,
                               k_visibility_texture_unit);
    }
  }
  if (road_uniforms.snow_coverage != Shader::InvalidUniform) {
    road_shader->set_uniform(road_uniforms.snow_coverage, feature.biome_snow_coverage);
  }
  if (road_uniforms.snow_color != Shader::InvalidUniform) {
    road_shader->set_uniform(road_uniforms.snow_color, feature.biome_snow_color);
  }
  PolygonOffsetScope const poly(-1.0F, -1.0F);
  draw_feature_models(prepared,
                      context,
                      road_shader,
                      road_uniforms.model,
                      road_uniforms.mvp,
                      road_uniforms.color,
                      road_uniforms.alpha,
                      road_uniforms.surface_kind);
}

} // namespace Render::GL::BackendPipelines
