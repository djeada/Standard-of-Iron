#include <cstdint>
#include <cstring>
#include <type_traits>

#include "render/backend/command_executor_common.h"

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
                                    PipelineDrawContext& context) {
  const auto& queue = context.queue;
  const QMatrix4x4& view_proj = context.frame.view_proj;
  const std::size_t batch_end = prepared.end();

  const auto execute = [&](const auto& head,
                           Shader* active_shader,
                           bool instanced_kind) {
    using Cmd = std::decay_t<decltype(head)>;
    if (head.mesh == nullptr || active_shader == nullptr) {
      return;
    }

    if (context.state.polygon_offset_enabled) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      context.state.polygon_offset_enabled = false;
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
          active_shader == m_banner_pipeline->shader()) {
        m_banner_pipeline->draw_banners(prepared,
                                        head,
                                        is_transparent,
                                        m_context.resources() != nullptr ? m_context.resources()->white() : nullptr,
                                        context);
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
      m_mesh_instancing_pipeline->flush(context.meshes);
      shader->set_uniform(uniforms->instanced, false);
    } else if (context.meshes.bind(*head.mesh)) {
      const bool upload_mvp = uniforms->view_proj == Shader::InvalidUniform;
      for (std::size_t j = prepared.start; j < batch_end; ++j) {
        const auto& single = std::get<Cmd>(queue.get_sorted(j));
        shader->set_uniform(uniforms->model, world_of(single));
        if (upload_mvp) {
          shader->set_uniform(uniforms->mvp, view_proj * world_of(single));
        }
        shader->set_uniform(uniforms->color, single.color);
        shader->set_uniform(uniforms->alpha, single.alpha);
        context.meshes.draw_bound(*head.mesh);
      }
      context.meshes.unbind();
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
      (texture != nullptr) ? texture : (m_context.resources() != nullptr ? m_context.resources()->white() : nullptr);
  if ((tex_to_use != nullptr) && tex_to_use != m_gl_state.bound_texture) {
    tex_to_use->bind(0);
    m_gl_state.bound_texture = tex_to_use;
  }
}

auto Backend::bind_mesh_shader(Shader* active_shader,
                               Texture* texture,
                               int material_id,
                               bool want_instanced,
                               PipelineDrawContext& context) -> MeshShaderBinding {
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

  if (m_gl_state.bound_shader != shader) {
    shader->use();
    shader->set_uniform(uniforms->view_proj, context.frame.view_proj);
    shader->set_uniform(uniforms->time, m_animation_time);
    shader->set_uniform(uniforms->light_dir, m_light_dir);
    shader->set_uniform(uniforms->ambient_strength, m_ambient_strength);
    shader->set_uniform(uniforms->camera_pos, (*context.frame.camera).get_position());
    bind_material_detail(*shader, *uniforms, m_context.resources());
    m_gl_state.bound_shader = shader;
  }

  bind_mesh_texture(texture);
  shader->set_uniform(uniforms->texture, 0);
  shader->set_uniform(uniforms->use_texture, texture != nullptr);
  shader->set_uniform(uniforms->material_id, material_id);
  shader->set_uniform(uniforms->instanced, instanced);
  return {.shader = shader, .uniforms = uniforms, .instanced = instanced};
}

auto Backend::supports_static_batch() const noexcept -> bool {
  return m_building_merged_shader != nullptr && m_building_ghost_shader != nullptr &&
         m_mesh_instancing_pipeline != nullptr &&
         m_mesh_instancing_pipeline->is_initialized() &&
         m_directional_shadow_depth_instanced_shader != nullptr;
}

auto Backend::prewarm_static_meshes(
    std::span<const std::shared_ptr<const MergedBuildingMesh>> meshes) -> bool {
  if (!supports_static_batch()) {
    return false;
  }
  bool ready = true;
  for (const auto& mesh : meshes) {
    if (mesh != nullptr && !mesh->indices.empty()) {
      ready = m_mesh_instancing_pipeline->ensure_merged_uploaded(*mesh) && ready;
    }
  }
  return ready;
}

void Backend::execute_static_batch(const StaticBuildingBatch& batch,
                                   PipelineDrawContext& context) {
  const auto& instances = batch.instances();
  std::size_t byte_offset = 0;
  if (instances.empty() || !m_mesh_instancing_pipeline->upload(
                               instances.data(),
                               instances.size() * sizeof(BuildingInstanceGpu),
                               byte_offset)) {
    return;
  }
  if (context.state.polygon_offset_enabled) {
    glDisable(GL_POLYGON_OFFSET_FILL);
    context.state.polygon_offset_enabled = false;
  }
  for (const StaticBatchDraw& draw : batch.draws()) {
    Shader* const shader =
        draw.ghost ? m_building_ghost_shader : m_building_merged_shader;
    for (const MergedBuildingRange& range : draw.mesh->ranges) {
      Texture* const texture =
          range.texture != nullptr ? range.texture : draw.default_texture;
      if (bind_mesh_shader(shader, texture, 0, false, context).shader == nullptr) {
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

namespace {

[[nodiscard]] auto mix_shadow_signature(std::uint64_t seed,
                                        std::uint64_t value) -> std::uint64_t {
  seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U);
  return seed;
}

[[nodiscard]] auto float_bits(float value) -> std::uint64_t {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

} // namespace

auto Backend::collect_static_batch_shadow(
    const StaticBuildingBatch& batch, const ShadowCascadeCull& cull) -> std::uint64_t {
  m_static_shadow_instances.clear();
  m_static_shadow_draws.clear();
  m_static_shadow_triangles = 0;
  std::uint64_t signature = 0;
  for (const StaticBatchDraw& draw : batch.draws()) {
    auto const first = static_cast<std::uint32_t>(m_static_shadow_instances.size());
    for (std::uint32_t index = draw.first; index < draw.first + draw.count; ++index) {
      const StaticBatchBounds& bounds = batch.bounds()[index];

      if (batch.instances()[index].state[2] <= 0.0F &&
          bounds.radius >= cull.min_caster_radius &&
          cull.accepts(bounds.center, bounds.radius)) {
        const BuildingInstanceGpu& instance = batch.instances()[index];
        m_static_shadow_instances.push_back(instance);
        signature = mix_shadow_signature(
            signature,
            static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(draw.mesh)));
        for (const float* column :
             {instance.model_col0, instance.model_col1, instance.model_col2}) {
          for (int component = 0; component < 4; ++component) {
            signature = mix_shadow_signature(signature, float_bits(column[component]));
          }
        }
      }
    }
    auto const count =
        static_cast<std::uint32_t>(m_static_shadow_instances.size()) - first;
    if (count == 0U) {
      continue;
    }
    m_static_shadow_triangles += (draw.mesh->indices.size() / 3U) * count;
    if (!m_static_shadow_draws.empty() &&
        m_static_shadow_draws.back().mesh == draw.mesh) {
      m_static_shadow_draws.back().count += count;
    } else {
      m_static_shadow_draws.push_back(
          StaticBatchDraw{.mesh = draw.mesh, .first = first, .count = count});
    }
  }
  return mix_shadow_signature(signature, m_static_shadow_instances.size());
}

void Backend::draw_collected_static_batch_shadow() {
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

void Backend::draw_static_batch_shadow(const StaticBuildingBatch& batch,
                                       const ShadowCascadeCull& cull) {
  (void)collect_static_batch_shadow(batch, cull);
  draw_collected_static_batch_shadow();
}

} // namespace Render::GL
