#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QVector2D>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "render/backend/backend.h"
#include "render/backend/directional_shadow_block.h"
#include "render/backend/mesh_instancing_pipeline.h"
#include "render/backend/rigged_cull_pipeline.h"
#include "render/backend/shadow_cascade_fit.h"
#include "render/gl/ubo_bindings.h"
#include "render/gl/rigged_mesh.h"
#include "render/frame/draw_queue.h"
#include "render/gl/gl_resource_tracking.h"
#include "render/gl/mesh.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader.h"
#include "scene/camera.h"

namespace Render::GL {

namespace {

constexpr float k_shadow_min_caster_texels = 0.75F;
constexpr double k_shadow_cache_triangles_per_texel = 0.1;
constexpr float k_shadow_flat_terrain_relief = 0.30F;
constexpr std::size_t k_shadow_min_instanced_run = 2;
constexpr float k_shadow_receiver_margin_texels = 16.0F;

[[nodiscard]] auto shadow_content_mix(std::uint64_t seed,
                                      std::uint64_t value) -> std::uint64_t {
  seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U);
  return seed;
}

[[nodiscard]] auto shadow_float_bits(float value) -> std::uint64_t {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

auto allocate_shadow_array(QOpenGLFunctions_3_3_Core& gl,
                           TextureHandle& texture,
                           int resolution,
                           int layers) -> void {
  texture = TextureHandle::create();
  gl.glBindTexture(GL_TEXTURE_2D_ARRAY, texture.id());
  gl.glTexImage3D(GL_TEXTURE_2D_ARRAY,
                  0,
                  GL_DEPTH_COMPONENT24,
                  resolution,
                  resolution,
                  layers,
                  0,
                  GL_DEPTH_COMPONENT,
                  GL_FLOAT,
                  nullptr);
  note_texture_storage(texture_transfer_bytes(static_cast<std::size_t>(resolution),
                                              static_cast<std::size_t>(resolution),
                                              static_cast<std::size_t>(layers) * 4U),
                       false);
  gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  const std::array<GLfloat, 4> border{1.0F, 1.0F, 1.0F, 1.0F};
  gl.glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border.data());
  gl.glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

auto near_cascade_split(int cascades, int resolution) -> int {
  if (cascades <= 2 || resolution <= 512) {
    return cascades;
  }
  return 2;
}

} // namespace

void Backend::release_directional_shadow_resources() {
  m_directional_shadow_texture.reset();
  m_directional_shadow_far_texture.reset();
  m_directional_shadow_fbo.reset();
  m_directional_shadow_static_texture.reset();
  m_directional_shadow_static_far_texture.reset();
  m_directional_shadow_cache_fbo.reset();
  m_shadow_static_layers.fill(ShadowStaticLayer{});
  m_directional_shadow_compare_sampler.reset();
  m_directional_shadow_depth_sampler.reset();
  m_directional_shadow_fallback_texture.reset();
  m_directional_shadow_resolution = 0;
  m_directional_shadow_far_resolution = 0;
  m_directional_shadow_cascades = 0;
  m_directional_shadow_near_cascades = 0;
}

auto Backend::directional_shadow_fallback_texture() -> GLuint {
  if (m_directional_shadow_fallback_texture.valid()) {
    return m_directional_shadow_fallback_texture.id();
  }
  m_directional_shadow_fallback_texture = TextureHandle::create();
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_directional_shadow_fallback_texture.id());
  const GLfloat lit_depth = 1.0F;
  glTexImage3D(GL_TEXTURE_2D_ARRAY,
               0,
               GL_DEPTH_COMPONENT24,
               1,
               1,
               1,
               0,
               GL_DEPTH_COMPONENT,
               GL_FLOAT,
               &lit_depth);
  note_texture_storage(texture_transfer_bytes(1U, 1U, 4U), false);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(
      GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  return m_directional_shadow_fallback_texture.id();
}

void Backend::bind_directional_shadow_textures() {
  const GLuint near_texture = m_directional_shadow_texture.valid()
                                  ? m_directional_shadow_texture.id()
                                  : directional_shadow_fallback_texture();
  const GLuint far_texture = m_directional_shadow_far_texture.valid()
                                 ? m_directional_shadow_far_texture.id()
                                 : near_texture;
  glActiveTexture(GL_TEXTURE0 + TextureUnit::directional_shadow_map);
  glBindTexture(GL_TEXTURE_2D_ARRAY, near_texture);
  glActiveTexture(GL_TEXTURE0 + TextureUnit::directional_shadow_map_far);
  glBindTexture(GL_TEXTURE_2D_ARRAY, far_texture);
  glActiveTexture(GL_TEXTURE0 + TextureUnit::directional_shadow_depth);
  glBindTexture(GL_TEXTURE_2D_ARRAY, near_texture);
  glActiveTexture(GL_TEXTURE0);
}

void Backend::park_directional_shadow_textures() {
  const GLuint parked = directional_shadow_fallback_texture();
  glActiveTexture(GL_TEXTURE0 + TextureUnit::directional_shadow_map);
  glBindTexture(GL_TEXTURE_2D_ARRAY, parked);
  glActiveTexture(GL_TEXTURE0 + TextureUnit::directional_shadow_map_far);
  glBindTexture(GL_TEXTURE_2D_ARRAY, parked);
  glActiveTexture(GL_TEXTURE0 + TextureUnit::directional_shadow_depth);
  glBindTexture(GL_TEXTURE_2D_ARRAY, parked);
  glActiveTexture(GL_TEXTURE0);
}

void Backend::ensure_directional_shadow_resources(int resolution, int cascades) {
  resolution = std::clamp(resolution, 512, 4096);
  cascades = std::clamp(cascades, 1, 4);
  const int near_cascades = near_cascade_split(cascades, resolution);
  const int far_cascades = cascades - near_cascades;
  const int far_resolution = far_cascades > 0 ? std::max(512, resolution / 2) : 0;

  if (m_directional_shadow_texture.valid() &&
      m_directional_shadow_resolution == resolution &&
      m_directional_shadow_cascades == cascades &&
      m_directional_shadow_near_cascades == near_cascades &&
      m_directional_shadow_far_resolution == far_resolution) {
    return;
  }

  release_directional_shadow_resources();
  allocate_shadow_array(*this, m_directional_shadow_texture, resolution, near_cascades);
  allocate_shadow_array(
      *this, m_directional_shadow_static_texture, resolution, near_cascades);
  if (far_cascades > 0) {
    allocate_shadow_array(
        *this, m_directional_shadow_far_texture, far_resolution, far_cascades);
    allocate_shadow_array(
        *this, m_directional_shadow_static_far_texture, far_resolution, far_cascades);
  }

  const std::array<GLfloat, 4> border{1.0F, 1.0F, 1.0F, 1.0F};
  if (!m_directional_shadow_compare_sampler) {
    m_directional_shadow_compare_sampler = SamplerHandle::create();
    glSamplerParameteri(
        m_directional_shadow_compare_sampler.id(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(
        m_directional_shadow_compare_sampler.id(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(m_directional_shadow_compare_sampler.id(),
                        GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_BORDER);
    glSamplerParameteri(m_directional_shadow_compare_sampler.id(),
                        GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(m_directional_shadow_compare_sampler.id(),
                         GL_TEXTURE_BORDER_COLOR,
                         border.data());
    glSamplerParameteri(m_directional_shadow_compare_sampler.id(),
                        GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(
        m_directional_shadow_compare_sampler.id(), GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  }
  if (!m_directional_shadow_depth_sampler) {
    m_directional_shadow_depth_sampler = SamplerHandle::create();
    glSamplerParameteri(
        m_directional_shadow_depth_sampler.id(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(
        m_directional_shadow_depth_sampler.id(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(
        m_directional_shadow_depth_sampler.id(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(
        m_directional_shadow_depth_sampler.id(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(m_directional_shadow_depth_sampler.id(),
                         GL_TEXTURE_BORDER_COLOR,
                         border.data());
    glSamplerParameteri(
        m_directional_shadow_depth_sampler.id(), GL_TEXTURE_COMPARE_MODE, GL_NONE);
  }
  glBindSampler(static_cast<GLuint>(TextureUnit::directional_shadow_map),
                m_directional_shadow_compare_sampler.id());
  glBindSampler(static_cast<GLuint>(TextureUnit::directional_shadow_map_far),
                m_directional_shadow_compare_sampler.id());
  glBindSampler(static_cast<GLuint>(TextureUnit::directional_shadow_depth),
                m_directional_shadow_depth_sampler.id());

  m_directional_shadow_fbo = FramebufferHandle::create();
  m_directional_shadow_cache_fbo = FramebufferHandle::create();
  m_shadow_static_layers.fill(ShadowStaticLayer{});
  m_directional_shadow_resolution = resolution;
  m_directional_shadow_far_resolution = far_resolution;
  m_directional_shadow_cascades = cascades;
  m_directional_shadow_near_cascades = near_cascades;
}

void Backend::upload_directional_shadow_block(const DirectionalShadowBlock& block) {
  if (!m_directional_shadow_ubo) {
    return;
  }
  const auto packed = block.packed_std140();
  glBindBuffer(GL_UNIFORM_BUFFER, m_directional_shadow_ubo.id());
  glBufferSubData(GL_UNIFORM_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(sizeof(float) * packed.size()),
                  packed.data());
  note_buffer_transfer(static_cast<std::size_t>(sizeof(float) * packed.size()));
  glBindBufferBase(GL_UNIFORM_BUFFER,
                   k_directional_shadow_binding_point,
                   m_directional_shadow_ubo.id());
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Backend::publish_directional_shadow_block(
    const Camera& cam,
    int cascade_count,
    const std::array<float, k_max_shadow_cascades>& cascade_texel_world,
    const std::array<float, k_max_shadow_cascades>& cascade_depth_span) {
  const Render::DirectionalShadowSettings& settings = m_shadow_settings;
  DirectionalShadowBlock block;
  for (int cascade = 0; cascade < k_max_shadow_cascades; ++cascade) {
    const auto slot = static_cast<std::size_t>(cascade);
    block.light_view_projection[slot] = m_directional_shadow_matrices[slot];
    block.split_distances[slot] = m_directional_shadow_splits[slot];
  }
  block.enabled = 1.0F;
  block.cascade_count = static_cast<float>(cascade_count);
  block.shadow_map_texel_size =
      1.0F / static_cast<float>(m_directional_shadow_resolution);
  block.far_shadow_map_texel_size =
      m_directional_shadow_far_resolution > 0
          ? 1.0F / static_cast<float>(m_directional_shadow_far_resolution)
          : block.shadow_map_texel_size;
  block.near_cascade_count = static_cast<float>(m_directional_shadow_near_cascades);

  block.pcf_radius = 0.0F;
  block.camera_position = cam.get_position();
  block.depth_bias = settings.depth_bias;
  block.normal_bias = settings.normal_bias;
  block.cascade_blend = settings.cascade_blend;
  block.cascade_texel_world = cascade_texel_world;
  block.cascade_depth_span = cascade_depth_span;

  const auto complete = block.packed_std140();
  glBindBuffer(GL_UNIFORM_BUFFER, m_directional_shadow_ubo.id());
  glBufferData(GL_UNIFORM_BUFFER,
               static_cast<GLsizeiptr>(sizeof(float) * complete.size()),
               nullptr,
               GL_DYNAMIC_DRAW);
  note_buffer_storage(static_cast<std::size_t>(sizeof(float) * complete.size()), false);
  glBufferSubData(GL_UNIFORM_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(sizeof(float) * complete.size()),
                  complete.data());
  note_buffer_transfer(static_cast<std::size_t>(sizeof(float) * complete.size()));
  glBindBufferBase(GL_UNIFORM_BUFFER,
                   k_directional_shadow_binding_point,
                   m_directional_shadow_ubo.id());
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

auto Backend::measure_shadow_receiver_slab(const DrawQueue& queue)
    -> ShadowReceiverSlab {
  float slab_min = std::numeric_limits<float>::max();
  float slab_max = std::numeric_limits<float>::lowest();
  if (m_shadow_terrain_height_cache.size() > 4096U) {
    m_shadow_terrain_height_cache.clear();
  }
  for (const auto& item : queue.items()) {
    const auto* terrain = std::get_if<TerrainSurfaceCmd>(&item);
    if (terrain == nullptr || terrain->mesh == nullptr || terrain->horizon_dressing) {
      continue;
    }
    auto found = m_shadow_terrain_height_cache.find(terrain->mesh);
    if (found == m_shadow_terrain_height_cache.end()) {
      float local_min = std::numeric_limits<float>::max();
      float local_max = std::numeric_limits<float>::lowest();
      for (const auto& vertex : terrain->mesh->get_vertices()) {
        local_min = std::min(local_min, vertex.position[1]);
        local_max = std::max(local_max, vertex.position[1]);
      }
      if (local_min > local_max) {
        local_min = local_max = 0.0F;
      }
      found = m_shadow_terrain_height_cache
                  .emplace(terrain->mesh, std::pair<float, float>{local_min, local_max})
                  .first;
    }

    const float y_a =
        terrain->model.map(QVector3D(0.0F, found->second.first, 0.0F)).y();
    const float y_b =
        terrain->model.map(QVector3D(0.0F, found->second.second, 0.0F)).y();
    slab_min = std::min({slab_min, y_a, y_b});
    slab_max = std::max({slab_max, y_a, y_b});
  }
  return finish_shadow_receiver_slab(slab_min, slab_max);
}

void Backend::collect_shadow_static_casters(const DrawQueue& queue) {
  const auto add_static_caster =
      [this](Mesh* mesh, const QMatrix4x4& model, bool terrain = false) {
        if (mesh == nullptr) {
          return;
        }
        ShadowStaticCaster caster;
        caster.mesh = mesh;
        caster.model = &model;
        caster.terrain = terrain;
        caster.world_center = model.map(mesh->bounds_center());
        const float scale = std::max({model.column(0).toVector3D().length(),
                                      model.column(1).toVector3D().length(),
                                      model.column(2).toVector3D().length()});
        caster.world_radius = mesh->bounds_radius() * scale;
        m_shadow_static_casters.push_back(caster);
      };

  m_shadow_static_casters.clear();
  for (const auto& item : queue.items()) {
    if (const auto* mesh = std::get_if<MeshCmd>(&item)) {
      if (mesh->alpha >= k_opaque_threshold && mesh->shader != m_shadow_shader) {
        add_static_caster(mesh->mesh, mesh->model);
      }
    } else if (const auto* part = std::get_if<DrawPartCmd>(&item)) {
      if (part->alpha >= k_opaque_threshold) {
        add_static_caster(part->mesh, part->world);
      }
    } else if (const auto* terrain = std::get_if<TerrainSurfaceCmd>(&item)) {
      if (terrain->horizon_dressing) {
        continue;
      }
      const auto relief = m_shadow_terrain_height_cache.find(terrain->mesh);
      if (relief != m_shadow_terrain_height_cache.end() &&
          relief->second.second - relief->second.first < k_shadow_flat_terrain_relief) {
        continue;
      }
      add_static_caster(terrain->mesh, terrain->model, true);
    }
  }
  std::sort(m_shadow_static_casters.begin(),
            m_shadow_static_casters.end(),
            [](const ShadowStaticCaster& lhs, const ShadowStaticCaster& rhs) {
              return lhs.mesh < rhs.mesh;
            });
}

auto Backend::begin_directional_shadow_pass() -> ShadowPassRestore {
  ShadowPassRestore restore;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &restore.framebuffer);
  glGetIntegerv(GL_VIEWPORT, restore.viewport.data());
  restore.blend = glIsEnabled(GL_BLEND) != 0U;
  restore.cull = glIsEnabled(GL_CULL_FACE) != 0U;
  glBindFramebuffer(GL_FRAMEBUFFER, m_directional_shadow_fbo.id());
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.0F, 4.0F);
  return restore;
}

void Backend::end_directional_shadow_pass(const ShadowPassRestore& restore) {
  glDisable(GL_POLYGON_OFFSET_FILL);
  glCullFace(GL_BACK);
  if (!restore.cull) {
    glDisable(GL_CULL_FACE);
  }
  if (restore.blend) {
    glEnable(GL_BLEND);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(restore.framebuffer));
  glViewport(restore.viewport[0],
             restore.viewport[1],
             restore.viewport[2],
             restore.viewport[3]);
}

auto Backend::shadow_cascade_target(int cascade) const -> ShadowCascadeTarget {
  ShadowCascadeTarget target;
  target.cascade = cascade;
  target.far = cascade >= m_directional_shadow_near_cascades;
  target.resolution = target.far ? m_directional_shadow_far_resolution
                                 : m_directional_shadow_resolution;
  target.texture = target.far ? m_directional_shadow_far_texture.id()
                              : m_directional_shadow_texture.id();
  target.static_texture = target.far ? m_directional_shadow_static_far_texture.id()
                                     : m_directional_shadow_static_texture.id();
  target.layer = target.far ? cascade - m_directional_shadow_near_cascades : cascade;
  return target;
}

void Backend::partition_shadow_cascade_casters(const ShadowCascadeCull& cull) {
  m_shadow_cascade_casters.clear();
  m_shadow_cascade_terrain_casters.clear();
  for (const auto& caster : m_shadow_static_casters) {
    if (caster.world_radius >= cull.min_caster_radius &&
        cull.accepts(caster.world_center, caster.world_radius)) {
      (caster.terrain ? m_shadow_cascade_terrain_casters : m_shadow_cascade_casters)
          .push_back(&caster);
    }
  }
}

void Backend::bind_shadow_depth_shader(ShadowDepthBinding& binding,
                                       Shader* shader,
                                       Shader::UniformHandle light_vp_handle) {
  if (binding.bound == shader) {
    return;
  }
  shader->use();
  shader->set_uniform(light_vp_handle, binding.light_view_projection);
  binding.bound = shader;
}

void Backend::draw_shadow_casters(const std::vector<const ShadowStaticCaster*>& casters,
                                  ShadowDepthBinding& binding) {
  const bool can_instance = supports_static_batch();
  for (std::size_t index = 0; index < casters.size();) {
    Mesh* const mesh = casters[index]->mesh;
    std::size_t run_end = index + 1;
    while (run_end < casters.size() && casters[run_end]->mesh == mesh) {
      ++run_end;
    }
    const std::size_t run = run_end - index;

    if (can_instance && run >= k_shadow_min_instanced_run) {
      bind_shadow_depth_shader(binding,
                               m_directional_shadow_depth_instanced_shader,
                               m_shadow_depth_instanced_light_vp);
      m_mesh_instancing_pipeline->begin_batch(mesh);
      for (std::size_t j = index; j < run_end; ++j) {
        m_mesh_instancing_pipeline->accumulate(
            *casters[j]->model, QVector3D(1.0F, 1.0F, 1.0F), 1.0F);
      }
      m_mesh_instancing_pipeline->flush();
      m_last_playback_stats.shadow_static_instanced_draws += 1;
      m_last_playback_stats.shadow_static_instanced_instances += run;
    } else {
      bind_shadow_depth_shader(
          binding, m_directional_shadow_depth_shader, m_shadow_depth_light_vp);
      for (std::size_t j = index; j < run_end; ++j) {
        m_directional_shadow_depth_shader->set_uniform(m_shadow_depth_model,
                                                       *casters[j]->model);
        mesh->draw();
        m_last_playback_stats.shadow_static_single_draws += 1;
      }
    }
    index = run_end;
  }
}

void Backend::render_shadow_cascade_static(const ShadowCascadeTarget& target,
                                           const ShadowCascadeCull& cull,
                                           const StaticBuildingBatch* static_batch,
                                           ShadowDepthBinding& binding) {
  std::uint64_t static_content = 0;
  std::size_t static_triangles = 0;
  for (const ShadowStaticCaster* caster : m_shadow_cascade_terrain_casters) {
    static_triangles += caster->mesh->get_indices().size() / 3U;
    static_content = shadow_content_mix(static_content,
                                        reinterpret_cast<std::uintptr_t>(caster->mesh));
    for (int element = 0; element < 16; ++element) {
      static_content = shadow_content_mix(
          static_content, shadow_float_bits(caster->model->constData()[element]));
    }
  }
  if (static_batch != nullptr) {
    static_content = shadow_content_mix(
        static_content, collect_static_batch_shadow(*static_batch, cull));
    static_triangles += m_static_shadow_triangles;
  }

  const auto draw_static_casters = [&]() {
    draw_shadow_casters(m_shadow_cascade_terrain_casters, binding);
    if (static_batch != nullptr) {
      bind_shadow_depth_shader(binding,
                               m_directional_shadow_depth_instanced_shader,
                               m_shadow_depth_instanced_light_vp);
      draw_collected_static_batch_shadow();
    }
  };

  auto& static_layer = m_shadow_static_layers[static_cast<std::size_t>(target.cascade)];
  const auto cascade_texels =
      static_cast<double>(target.resolution) * static_cast<double>(target.resolution);
  const bool cache_worthwhile = static_cast<double>(static_triangles) >=
                                cascade_texels * k_shadow_cache_triangles_per_texel;
  if (!cache_worthwhile) {
    static_layer.valid = false;
    glFramebufferTextureLayer(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target.texture, 0, target.layer);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw_static_casters();
    ++m_last_playback_stats.shadow_static_cache_bypasses;
    return;
  }

  const bool static_layer_current =
      static_layer.valid && static_layer.content == static_content &&
      static_layer.light_vp == binding.light_view_projection;
  if (!static_layer_current) {
    glFramebufferTextureLayer(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target.static_texture, 0, target.layer);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw_static_casters();
    static_layer = ShadowStaticLayer{.light_vp = binding.light_view_projection,
                                     .content = static_content,
                                     .valid = true};
    ++m_last_playback_stats.shadow_static_cache_misses;
  } else {
    ++m_last_playback_stats.shadow_static_cache_hits;
  }

  glFramebufferTextureLayer(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target.texture, 0, target.layer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, m_directional_shadow_cache_fbo.id());
  glFramebufferTextureLayer(
      GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target.static_texture, 0, target.layer);
  glReadBuffer(GL_NONE);
  glBlitFramebuffer(0,
                    0,
                    target.resolution,
                    target.resolution,
                    0,
                    0,
                    target.resolution,
                    target.resolution,
                    GL_DEPTH_BUFFER_BIT,
                    GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, m_directional_shadow_fbo.id());
}

void Backend::render_shadow_cascade_rigged(const DrawQueue& queue,
                                           const Camera& cam,
                                           const ShadowCascadeTarget& target,
                                           const QMatrix4x4& light_view_projection,
                                           const ShadowCascadeRange& range) {
  const Render::DirectionalShadowSettings& settings = m_shadow_settings;

  const auto draw_single_rigged_shadow = [&](const RiggedCreatureCmd& rigged) {
    glBindBufferRange(GL_UNIFORM_BUFFER,
                      k_bone_palette_binding_point,
                      rigged.palette_ubo,
                      static_cast<GLintptr>(rigged.palette_offset),
                      static_cast<GLsizeiptr>(64U * sizeof(QMatrix4x4)));
    m_directional_shadow_rigged_shader->use();
    m_directional_shadow_rigged_shader->set_uniform("u_light_vp",
                                                    light_view_projection);
    m_directional_shadow_rigged_shader->set_uniform("u_model", rigged.world);
    m_directional_shadow_rigged_shader->set_uniform("u_variation_scale",
                                                    rigged.variation_scale);
    RiggedMesh* const cast_mesh =
        rigged.shadow_mesh != nullptr ? rigged.shadow_mesh : rigged.mesh;
    cast_mesh->draw();
    ++m_last_playback_stats.shadow_rigged_single_draws;
  };

  const auto rigged_casts_in_cascade = [&](const RiggedCreatureCmd& rigged,
                                           float camera_distance) {
    return rigged.mesh != nullptr && rigged.bone_palette != nullptr &&
           rigged.alpha >= k_opaque_threshold &&
           camera_distance <= range.far_distance + range.rigged_slack &&
           camera_distance + range.rigged_slack >= range.near_distance;
  };
  const auto rigged_camera_distance = [&](const RiggedCreatureCmd& rigged) {
    return (rigged.world.column(3).toVector3D() - cam.get_position()).length();
  };

  float caster_cutoff = std::numeric_limits<float>::max();
  if (settings.max_rigged_casters_per_cascade > 0) {
    const auto budget =
        static_cast<std::size_t>(settings.max_rigged_casters_per_cascade);
    thread_local std::vector<float> caster_distances;
    caster_distances.clear();
    for (const PreparedBatch& prepared : queue.prepared_batches()) {
      if (prepared.count == 0U || prepared.type != DrawCmdType::RiggedCreature) {
        continue;
      }
      for (std::size_t index = prepared.start; index < prepared.end(); ++index) {
        const auto& rigged = std::get<RiggedCreatureCmdIndex>(queue.get_sorted(index));
        const float camera_distance = rigged_camera_distance(rigged);
        if (rigged_casts_in_cascade(rigged, camera_distance)) {
          caster_distances.push_back(camera_distance);
        }
      }
    }
    if (caster_distances.size() > budget) {
      std::nth_element(caster_distances.begin(),
                       caster_distances.begin() + static_cast<std::ptrdiff_t>(budget),
                       caster_distances.end());
      caster_cutoff = caster_distances[budget];
      m_last_playback_stats.shadow_rigged_casters_dropped +=
          caster_distances.size() - budget;
    }
  }

  thread_local std::vector<const RiggedCreatureCmd*> visible_rigged;
  for (const PreparedBatch& prepared : queue.prepared_batches()) {
    if (prepared.count == 0U || prepared.type != DrawCmdType::RiggedCreature) {
      continue;
    }
    visible_rigged.clear();
    for (std::size_t index = prepared.start; index < prepared.end(); ++index) {
      const auto& rigged = std::get<RiggedCreatureCmdIndex>(queue.get_sorted(index));
      const float camera_distance = rigged_camera_distance(rigged);
      if (!rigged_casts_in_cascade(rigged, camera_distance) ||
          camera_distance >= caster_cutoff) {
        continue;
      }
      visible_rigged.push_back(&rigged);
    }

    if (m_rigged_cull_pipeline != nullptr &&
        m_rigged_cull_pipeline->has_shadow_path() && !visible_rigged.empty()) {
      if (m_rigged_cull_pipeline->draw_full_mesh_shadow(visible_rigged.data(),
                                                        visible_rigged.size(),
                                                        light_view_projection,
                                                        target.far)) {
        m_last_playback_stats.shadow_rigged_instanced_instances +=
            visible_rigged.size();
        m_last_playback_stats.shadow_rigged_instanced_draws +=
            m_rigged_cull_pipeline->last_stats().draw_calls;
        continue;
      }
      auto const extent = static_cast<float>(target.resolution);
      if (visible_rigged.size() >=
              BackendPipelines::RiggedCullPipeline::minimum_instances() &&
          m_rigged_cull_pipeline->draw_shadow(visible_rigged.data(),
                                              visible_rigged.size(),
                                              light_view_projection,
                                              QVector2D(extent, extent),
                                              target.far)) {
        m_last_playback_stats.shadow_rigged_instanced_instances +=
            visible_rigged.size();
        ++m_last_playback_stats.shadow_rigged_instanced_draws;
        continue;
      }
    }

    for (const RiggedCreatureCmd* rigged : visible_rigged) {
      if (rigged->palette_ubo != 0U) {
        draw_single_rigged_shadow(*rigged);
      }
    }
  }
}

void Backend::render_shadow_cascade_scatter(const DrawQueue& queue,
                                            const Camera& cam,
                                            const ShadowCascadeFrame& frame,
                                            const ShadowCascadeCull& cull) {
  bool shadow_polygon_offset_enabled = true;
  CommandExecutionContext shadow_context{queue,
                                         cam,
                                         frame.light_view,
                                         frame.light_projection,
                                         frame.light_view_projection,
                                         0.0F,
                                         shadow_polygon_offset_enabled};

  upload_frame_data(frame.light_view_projection, true);
  m_last_bound_shader = nullptr;
  m_last_bound_texture = nullptr;
  for (const PreparedBatch& prepared : queue.prepared_batches()) {
    if (prepared.count == 0) {
      continue;
    }
    const auto* scatter =
        std::get_if<TerrainScatterCmd>(&queue.get_sorted(prepared.start));

    if (scatter == nullptr ||
        scatter->species == TerrainScatterCmd::Species::FireCamp ||
        scatter->species == TerrainScatterCmd::Species::Grass ||
        !cull.accepts(scatter->bounds_center, scatter->bounds_radius)) {
      continue;
    }
    execute_scatter_commands(prepared, shadow_context);
  }
}

void Backend::render_shadow_cascade(const DrawQueue& queue,
                                    const Camera& cam,
                                    const ShadowCascadeTarget& target,
                                    const ShadowCascadeFrame& frame,
                                    const ShadowCascadeCull& cull,
                                    const ShadowCascadeRange& range) {
  glViewport(0, 0, target.resolution, target.resolution);
  partition_shadow_cascade_casters(cull);

  const StaticBuildingBatch* const static_batch =
      supports_static_batch() ? queue.static_batch() : nullptr;
  ShadowDepthBinding binding{.bound = nullptr,
                             .light_view_projection = frame.light_view_projection};
  render_shadow_cascade_static(target, cull, static_batch, binding);
  draw_shadow_casters(m_shadow_cascade_casters, binding);
  render_shadow_cascade_rigged(queue, cam, target, frame.light_view_projection, range);
  render_shadow_cascade_scatter(queue, cam, frame, cull);
}

void Backend::render_directional_shadows(const DrawQueue& queue, const Camera& cam) {
  const Render::DirectionalShadowSettings& settings = m_shadow_settings;
  const bool enabled = settings.enabled &&
                       m_environment_lighting.shadow_strength > 0.0F &&
                       m_directional_shadow_depth_shader != nullptr &&
                       m_directional_shadow_rigged_shader != nullptr;

  if (!enabled) {
    upload_directional_shadow_block(DirectionalShadowBlock{});
    bind_directional_shadow_textures();
    return;
  }

  const int cascade_count =
      std::clamp(settings.cascade_count, 1, k_max_shadow_cascades);
  ensure_directional_shadow_resources(settings.resolution, cascade_count);
  if (!m_directional_shadow_fbo || !m_directional_shadow_texture) {
    bind_directional_shadow_textures();
    return;
  }

  upload_directional_shadow_block(DirectionalShadowBlock{});

  bool invertible = false;
  const QMatrix4x4 view_projection = cam.get_view_projection_matrix();
  const QMatrix4x4 inverse_view_projection = view_projection.inverted(&invertible);
  if (!invertible) {
    bind_directional_shadow_textures();
    return;
  }

  const std::array<QVector3D, 5> view_rays =
      shadow_view_rays(inverse_view_projection, cam.get_forward_vector());
  const ShadowReceiverSlab slab = measure_shadow_receiver_slab(queue);

  const float camera_near = std::max(cam.get_near(), 0.05F);
  const float shadow_reach =
      std::max(camera_near + 1.0F, std::min(settings.distance, cam.get_far()));
  const ShadowRangeFit range = fit_shadow_distance_range(
      cam.get_position(), view_rays, slab.min_y, slab.max_y, camera_near, shadow_reach);
  m_directional_shadow_splits = compute_shadow_cascade_splits(
      range.near_distance, range.far_distance, cascade_count);

  m_shadow_light_direction = follow_shadow_light_direction(
      m_shadow_light_direction, m_environment_lighting.primary_direction.normalized());
  const ShadowLightBasis light = shadow_light_basis(m_shadow_light_direction);
  const ShadowSunThrow sun = shadow_sun_throw(light.direction);

  const ShadowPassRestore restore = begin_directional_shadow_pass();
  collect_shadow_static_casters(queue);
  park_directional_shadow_textures();

  std::array<float, k_max_shadow_cascades> cascade_texel_world{};
  std::array<float, k_max_shadow_cascades> cascade_depth_span{};

  float cascade_near = range.near_distance;
  for (int cascade = 0; cascade < cascade_count; ++cascade) {
    const auto slot = static_cast<std::size_t>(cascade);
    const float cascade_far = m_directional_shadow_splits[slot];
    const ShadowCascadeTarget target = shadow_cascade_target(cascade);
    const ShadowCascadeSphere sphere =
        fit_shadow_cascade_sphere(cam.get_position(),
                                  std::span<const QVector3D, 4>(view_rays.data(), 4),
                                  view_rays[4],
                                  cascade_near,
                                  cascade_far);
    const ShadowCascadeFrame frame =
        fit_shadow_cascade_frame(sphere, light, target.resolution);
    cascade_texel_world[slot] = frame.world_texel;
    cascade_depth_span[slot] = frame.depth_span;
    m_directional_shadow_matrices[slot] = frame.light_view_projection;

    const ShadowCascadeCull cull{
        .center = frame.center,
        .radius = frame.radius,
        .light_right = light.right,
        .light_up = light.up,
        .camera_position = cam.get_position(),
        .receiver_near = cascade == 0 ? 0.0F
                                      : m_directional_shadow_splits[slot - 1] *
                                            (1.0F - settings.cascade_blend),
        .receiver_far = cascade_far,
        .receiver_margin = frame.world_texel * k_shadow_receiver_margin_texels,
        .ground_height = slab.min_y,
        .shadow_throw = sun.throw_scale,
        .min_caster_radius = frame.world_texel * k_shadow_min_caster_texels,
    };
    render_shadow_cascade(queue,
                          cam,
                          target,
                          frame,
                          cull,
                          {.near_distance = cascade_near,
                           .far_distance = cascade_far,
                           .rigged_slack = sun.rigged_slack});
    cascade_near = cascade_far;
  }
  upload_frame_data(view_projection, false);
  m_last_bound_shader = nullptr;
  m_last_bound_texture = nullptr;
  end_directional_shadow_pass(restore);

  publish_directional_shadow_block(
      cam, cascade_count, cascade_texel_world, cascade_depth_span);
  bind_directional_shadow_textures();
}

} // namespace Render::GL
