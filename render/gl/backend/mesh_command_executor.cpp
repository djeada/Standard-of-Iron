#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

void Backend::execute_mesh_commands(const PreparedBatch& prepared,
                                    CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool& polygon_offset_enabled = context.polygon_offset_enabled;
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
  case MeshCmdIndex: {
    const auto& it = std::get<MeshCmdIndex>(cmd);
    if (it.mesh == nullptr) {
      break;
    }

    Shader* active_shader = (it.shader != nullptr) ? it.shader : m_basic_shader;
    if (active_shader == nullptr) {
      break;
    }

    if (polygon_offset_enabled) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      polygon_offset_enabled = false;
    }

    bool const is_shadow_shader = (active_shader == m_shadow_shader);
    std::optional<DepthMaskScope> shadow_depth_scope;
    std::optional<BlendScope> shadow_blend_scope;
    if (is_shadow_shader) {
      shadow_depth_scope.emplace(false);
      shadow_blend_scope.emplace(true);
    } else {
      glDepthMask(GL_TRUE);
    }

    bool const is_transparent = (!is_shadow_shader) && (it.alpha < 0.999F);
    std::optional<DepthMaskScope> transparent_depth_scope;
    std::optional<BlendScope> transparent_blend_scope;
    if (is_transparent) {
      transparent_depth_scope.emplace(false);
      transparent_blend_scope.emplace(true);
      glDepthFunc(GL_LEQUAL);
    }

    if (m_banner_pipeline != nullptr &&
        active_shader == m_banner_pipeline->m_banner_shader) {
      CullFaceScope const banner_cull(false);
      if (m_last_bound_shader != active_shader) {
        active_shader->use();
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.time,
                                   m_animation_time);
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.wind_strength,
                                   banner_wind_strength);
        active_shader->set_uniform(
            m_banner_pipeline->m_banner_uniforms.light_direction, m_light_dir);
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.camera_pos,
                                   cam.get_position());
        active_shader->set_uniform(
            m_banner_pipeline->m_banner_uniforms.ambient_strength,
            m_ambient_strength);
        m_last_bound_shader = active_shader;
      }

      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& single = std::get<MeshCmdIndex>(queue.get_sorted(j));
        QMatrix4x4 const mvp = view_proj * single.model;
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.mvp, mvp);
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.model,
                                   single.model);
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.color,
                                   single.color);

        QVector3D const trim_color =
            single.has_trim_color ? single.trim_color : (single.color * 0.7F);
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.trim_color,
                                   trim_color);
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.alpha,
                                   single.alpha);
        active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.use_texture,
                                   single.texture != nullptr);

        Texture* tex_to_use = (single.texture != nullptr)
                                  ? single.texture
                                  : (m_resources ? m_resources->white() : nullptr);
        if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
          tex_to_use->bind(0);
          m_last_bound_texture = tex_to_use;
          active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.texture, 0);
        }
        single.mesh->draw();
      }
      break;
    }

    auto* uniforms = m_character_pipeline
                         ? m_character_pipeline->resolve_uniforms(active_shader)
                         : nullptr;
    if (uniforms == nullptr) {
      break;
    }

    if (m_last_bound_shader != active_shader) {
      active_shader->use();
      if (uniforms->view_proj != Shader::InvalidUniform) {
        active_shader->set_uniform(uniforms->view_proj, view_proj);
      }
      const Shader::UniformHandle time_uniform =
          active_shader->optional_uniform_handle("u_time");
      if (time_uniform != Shader::InvalidUniform) {
        active_shader->set_uniform(time_uniform, m_animation_time);
      }
      if (uniforms->light_dir != Shader::InvalidUniform) {
        active_shader->set_uniform(uniforms->light_dir, m_light_dir);
      }
      if (uniforms->ambient_strength != Shader::InvalidUniform) {
        active_shader->set_uniform(uniforms->ambient_strength, m_ambient_strength);
      }
      m_last_bound_shader = active_shader;
    }

    const bool can_execute_prepared_batch =
        prepared.kind == PreparedBatchKind::MeshInstanced &&
        m_mesh_instancing_pipeline && m_mesh_instancing_pipeline->is_initialized() &&
        !is_transparent && !is_shadow_shader &&
        (uniforms->instanced != Shader::InvalidUniform ||
         uniforms->instanced_variant != nullptr);

    if (can_execute_prepared_batch) {
      const bool use_dedicated_variant = (uniforms->instanced_variant != nullptr);
      GL::Shader* batch_shader =
          use_dedicated_variant ? uniforms->instanced_variant : active_shader;

      if (use_dedicated_variant) {
        auto* inst_uniforms = m_character_pipeline
                                  ? m_character_pipeline->resolve_uniforms(batch_shader)
                                  : nullptr;
        if (inst_uniforms != nullptr) {
          batch_shader->use();
          if (inst_uniforms->view_proj != Shader::InvalidUniform) {
            batch_shader->set_uniform(inst_uniforms->view_proj, view_proj);
          }
          const Shader::UniformHandle time_uniform =
              batch_shader->optional_uniform_handle("u_time");
          if (time_uniform != Shader::InvalidUniform) {
            batch_shader->set_uniform(time_uniform, m_animation_time);
          }
          if (inst_uniforms->light_dir != Shader::InvalidUniform) {
            batch_shader->set_uniform(inst_uniforms->light_dir, m_light_dir);
          }
          if (inst_uniforms->ambient_strength != Shader::InvalidUniform) {
            batch_shader->set_uniform(inst_uniforms->ambient_strength,
                                      m_ambient_strength);
          }
          Texture* tex_to_use = (it.texture != nullptr)
                                    ? it.texture
                                    : (m_resources ? m_resources->white() : nullptr);
          if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
            tex_to_use->bind(0);
            m_last_bound_texture = tex_to_use;
            batch_shader->set_uniform(inst_uniforms->texture, 0);
          }
          batch_shader->set_uniform(inst_uniforms->use_texture, it.texture != nullptr);
          batch_shader->set_uniform(inst_uniforms->material_id, it.material_id);
          m_last_bound_shader = batch_shader;
        }
      } else {
        active_shader->set_uniform(uniforms->instanced, true);

        Texture* tex_to_use = (it.texture != nullptr)
                                  ? it.texture
                                  : (m_resources ? m_resources->white() : nullptr);
        if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
          tex_to_use->bind(0);
          m_last_bound_texture = tex_to_use;
          active_shader->set_uniform(uniforms->texture, 0);
        }
        active_shader->set_uniform(uniforms->use_texture, it.texture != nullptr);
        active_shader->set_uniform(uniforms->material_id, it.material_id);
      }

      m_mesh_instancing_pipeline->begin_batch(it.mesh, batch_shader, it.texture);
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& batch_it = std::get<MeshCmdIndex>(queue.get_sorted(j));
        m_mesh_instancing_pipeline->accumulate(
            batch_it.model, batch_it.color, batch_it.alpha, batch_it.material_id);
      }
      m_mesh_instancing_pipeline->flush();

      if (!use_dedicated_variant) {
        active_shader->set_uniform(uniforms->instanced, false);
      }
    } else {
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& single = std::get<MeshCmdIndex>(queue.get_sorted(j));

        active_shader->set_uniform(uniforms->model, single.model);
        if (uniforms->view_proj == Shader::InvalidUniform &&
            uniforms->mvp != Shader::InvalidUniform) {
          QMatrix4x4 const mvp = view_proj * single.model;
          active_shader->set_uniform(uniforms->mvp, mvp);
        }

        Texture* single_tex_to_use =
            (single.texture != nullptr)
                ? single.texture
                : (m_resources ? m_resources->white() : nullptr);
        if ((single_tex_to_use != nullptr) &&
            single_tex_to_use != m_last_bound_texture) {
          single_tex_to_use->bind(0);
          m_last_bound_texture = single_tex_to_use;
          active_shader->set_uniform(uniforms->texture, 0);
        }

        active_shader->set_uniform(uniforms->use_texture, single.texture != nullptr);
        active_shader->set_uniform(uniforms->color, single.color);
        active_shader->set_uniform(uniforms->alpha, single.alpha);
        active_shader->set_uniform(uniforms->material_id, single.material_id);
        if (uniforms->instanced != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms->instanced, false);
        }
        single.mesh->draw();
      }
    }
    if (is_transparent) {
      glDepthFunc(GL_LESS);
    }
    break;
  }
  case DrawPartCmdIndex: {

    const auto& part = std::get<DrawPartCmdIndex>(cmd);
    if (part.mesh == nullptr) {
      break;
    }

    const Render::ShaderQuality shader_quality =
        Render::GraphicsSettings::instance().features().shader_quality;
    Shader* active_shader =
        (part.material != nullptr) ? part.material->resolve(shader_quality) : nullptr;
    if (active_shader == nullptr) {
      active_shader = m_basic_shader;
    }
    if (active_shader == nullptr) {
      break;
    }

    if (polygon_offset_enabled) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      polygon_offset_enabled = false;
    }

    const float part_alpha = part.alpha;
    const bool is_transparent = part_alpha < k_opaque_threshold;

    std::optional<DepthMaskScope> transparent_depth_scope;
    std::optional<BlendScope> transparent_blend_scope;
    glDepthMask(GL_TRUE);
    if (is_transparent) {
      transparent_depth_scope.emplace(false);
      transparent_blend_scope.emplace(true);
      glDepthFunc(GL_LEQUAL);
    }

    auto* uniforms = m_character_pipeline
                         ? m_character_pipeline->resolve_uniforms(active_shader)
                         : nullptr;
    if (uniforms == nullptr) {
      if (is_transparent) {
        glDepthFunc(GL_LESS);
      }
      break;
    }

    if (m_last_bound_shader != active_shader) {
      active_shader->use();
      if (uniforms->view_proj != Shader::InvalidUniform) {
        active_shader->set_uniform(uniforms->view_proj, view_proj);
      }
      m_last_bound_shader = active_shader;
    }

    Texture* tex_to_use = (part.texture != nullptr)
                              ? part.texture
                              : (m_resources ? m_resources->white() : nullptr);
    if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
      tex_to_use->bind(0);
      m_last_bound_texture = tex_to_use;
      active_shader->set_uniform(uniforms->texture, 0);
    }
    active_shader->set_uniform(uniforms->use_texture, part.texture != nullptr);
    active_shader->set_uniform(uniforms->material_id, part.material_id);

    const bool can_execute_prepared_batch =
        prepared.kind == PreparedBatchKind::DrawPartInstanced &&
        m_mesh_instancing_pipeline && m_mesh_instancing_pipeline->is_initialized() &&
        !is_transparent && uniforms->instanced != Shader::InvalidUniform &&
        part.palette.empty();

    if (can_execute_prepared_batch) {

      active_shader->set_uniform(uniforms->instanced, true);

      m_mesh_instancing_pipeline->begin_batch(part.mesh, active_shader, part.texture);
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& batch_part = std::get<DrawPartCmdIndex>(queue.get_sorted(j));
        m_mesh_instancing_pipeline->accumulate(batch_part.world,
                                               batch_part.color,
                                               batch_part.alpha,
                                               batch_part.material_id);
      }
      m_mesh_instancing_pipeline->flush();

      active_shader->set_uniform(uniforms->instanced, false);
    } else {
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto& single_part = std::get<DrawPartCmdIndex>(queue.get_sorted(j));

        Texture* single_tex_to_use =
            (single_part.texture != nullptr)
                ? single_part.texture
                : (m_resources ? m_resources->white() : nullptr);
        if ((single_tex_to_use != nullptr) &&
            single_tex_to_use != m_last_bound_texture) {
          single_tex_to_use->bind(0);
          m_last_bound_texture = single_tex_to_use;
          active_shader->set_uniform(uniforms->texture, 0);
        }
        active_shader->set_uniform(uniforms->use_texture,
                                   single_part.texture != nullptr);
        active_shader->set_uniform(uniforms->material_id, single_part.material_id);
        active_shader->set_uniform(uniforms->model, single_part.world);
        if (uniforms->view_proj == Shader::InvalidUniform &&
            uniforms->mvp != Shader::InvalidUniform) {
          QMatrix4x4 const mvp = view_proj * single_part.world;
          active_shader->set_uniform(uniforms->mvp, mvp);
        }
        active_shader->set_uniform(uniforms->color, single_part.color);
        active_shader->set_uniform(uniforms->alpha, single_part.alpha);
        if (uniforms->instanced != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms->instanced, false);
        }
        single_part.mesh->draw();
      }
    }

    if (is_transparent) {
      glDepthFunc(GL_LESS);
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
