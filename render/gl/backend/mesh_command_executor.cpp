#include <type_traits>

#include "command_executor_common.h"

namespace Render::GL {

namespace {

auto world_of(const MeshCmd& cmd) -> const QMatrix4x4& {
  return cmd.model;
}

auto world_of(const DrawPartCmd& cmd) -> const QMatrix4x4& {
  return cmd.world;
}

} // namespace

void Backend::execute_mesh_commands(const PreparedBatch& prepared,
                                    CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const QMatrix4x4& view_proj = context.view_proj;
  const std::size_t batch_end = prepared.end();

  const auto execute = [&](const auto& head,
                           Shader* active_shader,
                           bool instanced_kind) {
    using Cmd = std::decay_t<decltype(head)>;
    if (head.mesh == nullptr || active_shader == nullptr) {
      return;
    }

    if (context.polygon_offset_enabled) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      context.polygon_offset_enabled = false;
    }

    const bool is_shadow_shader = (active_shader == m_shadow_shader);
    const bool is_transparent = !is_shadow_shader && head.alpha < k_opaque_threshold;
    std::optional<DepthMaskScope> depth_scope;
    std::optional<BlendScope> blend_scope;
    if (is_shadow_shader || is_transparent) {
      depth_scope.emplace(false);
      blend_scope.emplace(true);
    }

    if constexpr (std::is_same_v<Cmd, MeshCmd>) {
      if (m_banner_pipeline != nullptr &&
          active_shader == m_banner_pipeline->m_banner_shader) {
        const auto& banner = m_banner_pipeline->m_banner_uniforms;
        if (is_transparent) {
          glDepthFunc(GL_LEQUAL);
        }
        CullFaceScope const banner_cull(false);
        if (m_last_bound_shader != active_shader) {
          active_shader->use();
          active_shader->set_uniform(banner.time, m_animation_time);
          active_shader->set_uniform(banner.wind_strength,
                                     context.banner_wind_strength);
          active_shader->set_uniform(banner.light_direction, m_light_dir);
          active_shader->set_uniform(banner.camera_pos, context.cam.get_position());
          active_shader->set_uniform(banner.ambient_strength, m_ambient_strength);
          m_last_bound_shader = active_shader;
        }
        bind_mesh_texture(head.texture);
        active_shader->set_uniform(banner.texture, 0);
        active_shader->set_uniform(banner.use_texture, head.texture != nullptr);
        if (head.mesh->bind_vao()) {
          for (std::size_t j = prepared.start; j < batch_end; ++j) {
            const auto& single = std::get<MeshCmd>(queue.get_sorted(j));
            active_shader->set_uniform(banner.mvp, view_proj * single.model);
            active_shader->set_uniform(banner.model, single.model);
            active_shader->set_uniform(banner.color, single.color);
            active_shader->set_uniform(banner.trim_color,
                                       single.has_trim_color ? single.trim_color
                                                             : (single.color * 0.7F));
            active_shader->set_uniform(banner.alpha, single.alpha);
            head.mesh->draw_bound();
          }
          head.mesh->unbind_vao();
        }
        if (is_transparent) {
          glDepthFunc(GL_LESS);
        }
        return;
      }
    }

    if (is_transparent) {
      glDepthFunc(GL_LEQUAL);
    }
    const MeshShaderBinding binding =
        bind_mesh_shader(active_shader,
                         head.texture,
                         head.material_id,
                         instanced_kind && !is_transparent,
                         context);
    Shader* const shader = binding.shader;
    const auto* const uniforms = binding.uniforms;
    const bool instanced = binding.instanced;
    if (shader == nullptr) {
      if (is_transparent) {
        glDepthFunc(GL_LESS);
      }
      return;
    }

    if (instanced) {
      m_mesh_instancing_pipeline->begin_batch(head.mesh);
      for (std::size_t j = prepared.start; j < batch_end; ++j) {
        const auto& single = std::get<Cmd>(queue.get_sorted(j));
        m_mesh_instancing_pipeline->accumulate(
            world_of(single), single.color, single.alpha);
      }
      m_mesh_instancing_pipeline->flush();
      shader->set_uniform(uniforms->instanced, false);
    } else if (head.mesh->bind_vao()) {
      const bool upload_mvp = uniforms->view_proj == Shader::InvalidUniform;
      for (std::size_t j = prepared.start; j < batch_end; ++j) {
        const auto& single = std::get<Cmd>(queue.get_sorted(j));
        shader->set_uniform(uniforms->model, world_of(single));
        if (upload_mvp) {
          shader->set_uniform(uniforms->mvp, view_proj * world_of(single));
        }
        shader->set_uniform(uniforms->color, single.color);
        shader->set_uniform(uniforms->alpha, single.alpha);
        head.mesh->draw_bound();
      }
      head.mesh->unbind_vao();
    }

    if (is_transparent) {
      glDepthFunc(GL_LESS);
    }
  };

  const auto& cmd = queue.get_sorted(prepared.start);
  if (const auto* mesh = std::get_if<MeshCmd>(&cmd)) {
    execute(*mesh,
            (mesh->shader != nullptr) ? mesh->shader : m_basic_shader,
            prepared.kind == PreparedBatchKind::MeshInstanced);
  } else if (const auto* part = std::get_if<DrawPartCmd>(&cmd)) {
    Shader* const material_shader = (part->material != nullptr)
                                        ? part->material->resolve(m_shader_quality)
                                        : nullptr;
    execute(*part,
            (material_shader != nullptr) ? material_shader : m_basic_shader,
            prepared.kind == PreparedBatchKind::DrawPartInstanced);
  }
}

void Backend::bind_mesh_texture(Texture* texture) {
  Texture* const tex_to_use =
      (texture != nullptr) ? texture : (m_resources ? m_resources->white() : nullptr);
  if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
    tex_to_use->bind(0);
    m_last_bound_texture = tex_to_use;
  }
}

auto Backend::bind_mesh_shader(Shader* active_shader,
                               Texture* texture,
                               int material_id,
                               bool want_instanced,
                               CommandExecutionContext& context) -> MeshShaderBinding {
  auto* const base_uniforms =
      m_shader_uniform_cache ? m_shader_uniform_cache->resolve_uniforms(active_shader)
                             : nullptr;
  if (base_uniforms == nullptr) {
    return {};
  }

  Shader* shader = active_shader;
  auto* uniforms = base_uniforms;
  bool instanced = want_instanced && m_mesh_instancing_pipeline &&
                   m_mesh_instancing_pipeline->is_initialized();
  if (instanced && base_uniforms->instanced_variant != nullptr) {
    if (auto* variant_uniforms = m_shader_uniform_cache->resolve_uniforms(
            base_uniforms->instanced_variant)) {
      shader = base_uniforms->instanced_variant;
      uniforms = variant_uniforms;
    }
  }
  instanced = instanced && (shader != active_shader ||
                            uniforms->instanced != Shader::InvalidUniform);

  if (m_last_bound_shader != shader) {
    shader->use();
    shader->set_uniform(uniforms->view_proj, context.view_proj);
    shader->set_uniform(uniforms->time, m_animation_time);
    shader->set_uniform(uniforms->light_dir, m_light_dir);
    shader->set_uniform(uniforms->ambient_strength, m_ambient_strength);
    shader->set_uniform(uniforms->camera_pos, context.cam.get_position());
    bind_material_detail(*shader, *uniforms, m_resources.get());
    m_last_bound_shader = shader;
  }

  bind_mesh_texture(texture);
  shader->set_uniform(uniforms->texture, 0);
  shader->set_uniform(uniforms->use_texture, texture != nullptr);
  shader->set_uniform(uniforms->material_id, material_id);
  shader->set_uniform(uniforms->instanced, instanced);
  return {.shader = shader, .uniforms = uniforms, .instanced = instanced};
}

auto Backend::supports_static_batch() const noexcept -> bool {
  return m_building_merged_shader != nullptr && m_mesh_instancing_pipeline != nullptr &&
         m_mesh_instancing_pipeline->is_initialized() &&
         m_directional_shadow_depth_instanced_shader != nullptr;
}

void Backend::execute_static_batch(const StaticBuildingBatch& batch,
                                   CommandExecutionContext& context) {
  const auto& instances = batch.instances();
  std::size_t byte_offset = 0;
  if (instances.empty() || !m_mesh_instancing_pipeline->upload(
                               instances.data(),
                               instances.size() * sizeof(BuildingInstanceGpu),
                               byte_offset)) {
    return;
  }
  if (context.polygon_offset_enabled) {
    glDisable(GL_POLYGON_OFFSET_FILL);
    context.polygon_offset_enabled = false;
  }
  for (const StaticBatchDraw& draw : batch.draws()) {
    for (const MergedBuildingRange& range : draw.mesh->ranges) {
      Texture* const texture =
          range.texture != nullptr ? range.texture : draw.default_texture;
      if (bind_mesh_shader(m_building_merged_shader, texture, 0, false, context)
              .shader == nullptr) {
        return;
      }
      m_mesh_instancing_pipeline->draw_merged(
          *draw.mesh,
          byte_offset + std::size_t{draw.first} * sizeof(BuildingInstanceGpu),
          draw.count,
          range.first_index,
          range.index_count);
      m_last_playback_stats.static_batch_draws += 1;
    }
    m_last_playback_stats.static_batch_instances += draw.count;
  }
}

void Backend::draw_static_batch_shadow(const StaticBuildingBatch& batch,
                                       const ShadowCascadeCull& cull) {
  m_static_shadow_instances.clear();
  m_static_shadow_draws.clear();
  for (const StaticBatchDraw& draw : batch.draws()) {
    auto const first = static_cast<std::uint32_t>(m_static_shadow_instances.size());
    for (std::uint32_t index = draw.first; index < draw.first + draw.count; ++index) {
      const StaticBatchBounds& bounds = batch.bounds()[index];
      if (bounds.radius >= cull.min_caster_radius &&
          cull.accepts(bounds.center, bounds.radius)) {
        m_static_shadow_instances.push_back(batch.instances()[index]);
      }
    }
    auto const count =
        static_cast<std::uint32_t>(m_static_shadow_instances.size()) - first;
    if (count == 0U) {
      continue;
    }
    if (!m_static_shadow_draws.empty() &&
        m_static_shadow_draws.back().mesh == draw.mesh) {
      m_static_shadow_draws.back().count += count;
    } else {
      m_static_shadow_draws.push_back(
          StaticBatchDraw{.mesh = draw.mesh, .first = first, .count = count});
    }
  }
  std::size_t byte_offset = 0;
  if (m_static_shadow_instances.empty() ||
      !m_mesh_instancing_pipeline->upload(m_static_shadow_instances.data(),
                                          m_static_shadow_instances.size() *
                                              sizeof(BuildingInstanceGpu),
                                          byte_offset)) {
    return;
  }
  for (const StaticBatchDraw& draw : m_static_shadow_draws) {
    m_mesh_instancing_pipeline->draw_merged(
        *draw.mesh,
        byte_offset + std::size_t{draw.first} * sizeof(BuildingInstanceGpu),
        draw.count,
        0U,
        static_cast<std::uint32_t>(draw.mesh->indices.size()));
    m_last_playback_stats.shadow_static_instanced_draws += 1;
    m_last_playback_stats.shadow_static_instanced_instances += draw.count;
  }
}

} // namespace Render::GL
