#include "backend.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qopenglcontext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#include "backend/banner_pipeline.h"
#include "backend/combat_dust_pipeline.h"
#include "backend/cylinder_pipeline.h"
#include "backend/effects_pipeline.h"
#include "backend/fog_range.h"
#include "backend/ground_marker_pipeline.h"
#include "backend/healer_aura_pipeline.h"
#include "backend/healing_beam_pipeline.h"
#include "backend/mesh_instancing_pipeline.h"
#include "backend/mode_indicator_pipeline.h"
#include "backend/post_process_pipeline.h"
#include "backend/primitive_batch_pipeline.h"
#include "backend/rain_pipeline.h"
#include "backend/rigged_character_pipeline.h"
#include "backend/rigged_cull_pipeline.h"
#include "backend/shader_uniform_cache.h"
#include "backend/sky_box_pipeline.h"
#include "backend/terrain_pipeline.h"
#include "backend/vegetation_pipeline.h"
#include "backend/water_pipeline.h"
#include "buffer.h"
#include "decoration_gpu.h"
#include "directional_shadow_block.h"
#include "draw_tally.h"
#include "gl/resources.h"
#include "gl_lifetime.h"
#include "gl_resource_tracking.h"
#include "mesh.h"
#include "platform_gl.h"
#include "render/draw_queue.h"
#include "render/geom/mode_indicator.h"
#include "render/geom/selection_disc.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/graphics_settings.h"
#include "render/local_lighting.h"
#include "render/material.h"
#include "render/primitive_batch.h"
#include "render/profiling/frame_profile.h"
#include "render/rain_gpu.h"
#include "render/rigged_mesh.h"
#include "render_constants.h"
#include "scene/camera.h"
#include "shader.h"
#include "shadow_cascade_fit.h"
#include "state_scopes.h"
#include "texture.h"
#include "ubo_bindings.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {}

namespace {

std::atomic<int> g_live_backend_count{0};

auto retain_backend() -> int {
  return g_live_backend_count.fetch_add(1, std::memory_order_acq_rel) + 1;
}

auto release_backend() -> int {
  return g_live_backend_count.fetch_sub(1, std::memory_order_acq_rel) - 1;
}

} // namespace

Backend::Backend() {
  retain_backend();
}

Backend::Backend(ShaderQuality quality)
    : m_shader_quality(quality) {
  retain_backend();
}

Backend::~Backend() {
  const bool last_backend = release_backend() == 0;
  if (QOpenGLContext::currentContext() == nullptr) {
    for_each_pipeline_slot([](auto& slot) { (void)slot.release(); });
    if (last_backend) {
      SharedGeometryCache::instance().release_all();
    }
    return;
  }

  for (FrameGpuTiming& slot : m_frame_timings) {
    wait_for_frame_slot(slot);
  }

  if (last_backend) {
    SharedGeometryCache::instance().release_all();
  }
  for_each_pipeline_slot([](auto& slot) { slot.reset(); });
}

auto Backend::initialize() -> bool {

  if (!initializeOpenGLFunctions()) {
    qCritical() << "Backend::initialize() FAILED: QOpenGLFunctions_3_3_Core could not"
                   " be initialized. The current OpenGL context does not support"
                   " OpenGL 3.3 Core Profile. Check that GPU drivers are up to date"
                   " and that the application window has a valid Core Profile context.";
    return false;
  }

  apply_graphics_profile(Render::GraphicsSettings::instance().profile(), true);

  m_frame_ubo = BufferHandle::create();
  glBindBuffer(GL_UNIFORM_BUFFER, m_frame_ubo.id());
  glBufferData(GL_UNIFORM_BUFFER,
               static_cast<GLsizeiptr>(k_frame_data_bytes),
               nullptr,
               GL_DYNAMIC_DRAW);
  note_buffer_storage(k_frame_data_bytes, false);
  glBindBufferBase(GL_UNIFORM_BUFFER, k_frame_data_binding_point, m_frame_ubo.id());
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  m_environment_lighting_ubo = BufferHandle::create();
  glBindBuffer(GL_UNIFORM_BUFFER, m_environment_lighting_ubo.id());
  constexpr GLsizeiptr environment_ubo_size =
      sizeof(float) * EnvironmentLightingState::k_packed_float_count;
  glBufferData(GL_UNIFORM_BUFFER, environment_ubo_size, nullptr, GL_DYNAMIC_DRAW);
  note_buffer_storage(static_cast<std::size_t>(environment_ubo_size), false);
  glBindBufferBase(GL_UNIFORM_BUFFER,
                   k_environment_lighting_binding_point,
                   m_environment_lighting_ubo.id());
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  m_local_lighting_ubo = BufferHandle::create();
  glBindBuffer(GL_UNIFORM_BUFFER, m_local_lighting_ubo.id());
  constexpr GLsizeiptr local_lighting_ubo_size =
      sizeof(float) * Render::LocalLightingBlock::k_float_count;
  glBufferData(GL_UNIFORM_BUFFER, local_lighting_ubo_size, nullptr, GL_DYNAMIC_DRAW);
  note_buffer_storage(static_cast<std::size_t>(local_lighting_ubo_size), false);
  glBindBufferBase(
      GL_UNIFORM_BUFFER, k_local_lighting_binding_point, m_local_lighting_ubo.id());
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  m_directional_shadow_ubo = BufferHandle::create();
  glBindBuffer(GL_UNIFORM_BUFFER, m_directional_shadow_ubo.id());
  glBufferData(GL_UNIFORM_BUFFER,
               sizeof(float) * DirectionalShadowBlock::k_float_count,
               nullptr,
               GL_DYNAMIC_DRAW);
  note_buffer_storage(
      static_cast<std::size_t>(sizeof(float) * DirectionalShadowBlock::k_float_count),
      false);
  glBindBufferBase(
      GL_UNIFORM_BUFFER, k_directional_shadow_binding_point, m_directional_shadow_ubo.id());
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthRange(0.0, 1.0);
  glDepthMask(GL_TRUE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_resources = std::make_unique<ResourceManager>();
  if (!m_resources->initialize()) {
    qCritical()
        << "Backend::initialize() FAILED: ResourceManager initialization failed";
    return false;
  }

  m_shader_cache = std::make_unique<ShaderCache>();
  m_shader_cache->initialize_defaults();

  if (!create_subsystem(
          m_cylinder_pipeline, "CylinderPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_vegetation_pipeline, "VegetationPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_terrain_pipeline, "TerrainPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_shader_uniform_cache, "ShaderUniformCache", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_rigged_character_pipeline,
                        "RiggedCharacterPipeline",
                        this,
                        m_shader_cache.get())) {
    return false;
  }
  m_rigged_cull_pipeline = std::make_unique<BackendPipelines::RiggedCullPipeline>();
  m_rigged_cull_pipeline->set_shader_cache(m_shader_cache.get());
  if (!m_rigged_cull_pipeline->initialize()) {
    m_rigged_cull_pipeline.reset();
  }

  const unsigned int wear_volume =
      (m_resources != nullptr) ? m_resources->wear_volume() : 0U;
  if (m_rigged_character_pipeline) {
    m_rigged_character_pipeline->set_wear_volume(wear_volume);
  }
  if (m_rigged_cull_pipeline) {
    m_rigged_cull_pipeline->set_wear_volume(wear_volume);
  }

  if (!create_subsystem(m_water_pipeline, "WaterPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_effects_pipeline, "EffectsPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_primitive_batch_pipeline, "PrimitiveBatchPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_banner_pipeline, "BannerPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_healing_beam_pipeline, "HealingBeamPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_healer_aura_pipeline, "HealerAuraPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_combat_dust_pipeline, "CombatDustPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_rain_pipeline, "RainPipeline", this, m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_ground_marker_pipeline, "GroundMarkerPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_mode_indicator_pipeline, "ModeIndicatorPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_mesh_instancing_pipeline, "MeshInstancingPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_post_process_pipeline, "PostProcessPipeline", m_shader_cache.get())) {
    return false;
  }
  m_sky_box_pipeline =
      std::make_unique<BackendPipelines::SkyBoxPipeline>(m_shader_cache.get());
  if (!m_sky_box_pipeline->initialize()) {
    qWarning() << "Backend: SkyBoxPipeline unavailable, commander view keeps the flat"
                  " sky";
    m_sky_box_pipeline.reset();
  }

  m_basic_shader = m_shader_cache->get(QStringLiteral("basic"));
  m_grid_shader = m_shader_cache->get(QStringLiteral("grid"));
  m_shadow_shader = m_shader_cache->get(QStringLiteral("troop_shadow"));
  m_shadow_instanced_shader =
      m_shader_cache->get(QStringLiteral("troop_shadow_instanced"));
  m_directional_shadow_depth_shader = m_shader_cache->get_or_load(
      QStringLiteral(":/assets/shaders/directional_shadow_depth.vert"),
      QStringLiteral(":/assets/shaders/directional_shadow_depth.frag"));
  m_gpu_breakdown_enabled = qEnvironmentVariableIntValue("SOI_GPU_BREAKDOWN") != 0;
  m_directional_shadow_depth_instanced_shader = m_shader_cache->get_or_load(
      QStringLiteral(":/assets/shaders/directional_shadow_depth_instanced.vert"),
      QStringLiteral(":/assets/shaders/directional_shadow_depth.frag"));
  m_building_merged_shader =
      m_shader_cache->load(QStringLiteral("building_merged"),
                           QStringLiteral(":/assets/shaders/building_merged.vert"),
                           QStringLiteral(":/assets/shaders/basic_instanced.frag"),
                           QStringLiteral("#define SOI_OPAQUE_ONLY 1\n"));
  m_building_ghost_shader = m_shader_cache->get_or_load(
      QStringLiteral(":/assets/shaders/building_merged.vert"),
      QStringLiteral(":/assets/shaders/basic_instanced.frag"));
  m_directional_shadow_rigged_shader = m_shader_cache->get_or_load(
      QStringLiteral(":/assets/shaders/directional_shadow_rigged.vert"),
      QStringLiteral(":/assets/shaders/directional_shadow_depth.frag"));
  if (m_directional_shadow_depth_shader != nullptr) {
    m_shadow_depth_light_vp =
        m_directional_shadow_depth_shader->uniform_handle("u_light_vp");
    m_shadow_depth_model = m_directional_shadow_depth_shader->uniform_handle("u_model");
  }
  if (m_directional_shadow_depth_instanced_shader != nullptr) {
    m_shadow_depth_instanced_light_vp =
        m_directional_shadow_depth_instanced_shader->uniform_handle("u_light_vp");
  }
  if (m_directional_shadow_rigged_shader != nullptr) {
    (void)m_directional_shadow_rigged_shader->bind_uniform_block(
        "BonePalette", k_bone_palette_binding_point);
  }
  if (m_basic_shader == nullptr) {
    qCritical() << "Backend::initialize() FAILED: required basic shader missing";
    return false;
  }
  if (m_grid_shader == nullptr) {
    qCritical() << "Backend::initialize() FAILED: required grid shader missing";
    return false;
  }
  if (m_shadow_shader == nullptr) {
    qCritical() << "Backend::initialize() FAILED: required troop shadow shader missing";
    return false;
  }

  MaterialRegistry::instance().init(m_basic_shader, m_shadow_shader);

  apply_graphics_profile(Render::GraphicsSettings::instance().profile(), false);
  return true;
}

auto Backend::banner_mesh() const -> Mesh* {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->get_banner_mesh();
  }
  return nullptr;
}

auto Backend::banner_shader() const -> Shader* {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->m_banner_shader;
  }
  return nullptr;
}

namespace {

auto shader_tier_defines(Render::ShaderTier tier) -> QString {
  return QStringLiteral("#define SOI_QUALITY_TIER %1\n").arg(static_cast<int>(tier));
}

} // namespace

void Backend::sync_graphics_profile() {
  const auto& settings = Render::GraphicsSettings::instance();
  const std::uint32_t generation = settings.generation();
  if (m_graphics_profile_applied && generation == m_graphics_generation) {
    return;
  }
  m_graphics_generation = generation;
  apply_graphics_profile(settings.profile(), !m_graphics_profile_applied);
}

void Backend::apply_graphics_profile(const Render::GraphicsProfile& profile,
                                     bool initial) {
  m_graphics_generation = Render::GraphicsSettings::instance().generation();
  const bool tier_changed = initial || profile.shader_tier != m_shader_tier;
  m_shader_tier = profile.shader_tier;
  m_shadow_settings = profile.directional_shadows;
  Shader::set_global_defines(shader_tier_defines(m_shader_tier));
  if (tier_changed && !initial) {
    const std::size_t reloaded = Shader::reload_all();
    qInfo() << "Backend: shader tier" << static_cast<int>(m_shader_tier) << "-"
            << reloaded << "programs recompiled";
  }
  m_contact_shadow_uniforms_valid = false;

  if (!m_shadow_settings.enabled) {
    release_directional_shadow_resources();
  }

  if (m_post_process_pipeline != nullptr) {
    m_post_process_pipeline->set_passes(
        {.bloom = profile.post_process.bloom,
         .godrays = profile.post_process.godrays,
         .ambient_occlusion = profile.post_process.ambient_occlusion,
         .fxaa = profile.post_process.fxaa});
  }
  m_graphics_profile_applied = true;
}

void Backend::update_contact_shadow_uniforms() {
  const QVector3D sun = m_environment_lighting.primary_direction;
  QVector2D ground(-sun.x(), -sun.z());
  if (ground.lengthSquared() < 1e-6F) {
    ground = QVector2D(0.0F, 1.0F);
  }
  ground.normalize();
  const float cast_weight = m_shadow_settings.enabled ? 0.0F : 1.0F;

  if (m_contact_shadow_uniforms_valid &&
      qFuzzyCompare(m_contact_shadow_ground, ground) &&
      m_contact_shadow_cast_weight == cast_weight) {
    return;
  }
  m_contact_shadow_ground = ground;
  m_contact_shadow_cast_weight = cast_weight;
  m_contact_shadow_uniforms_valid = true;

  for (Shader* shader : {m_shadow_shader, m_shadow_instanced_shader}) {
    if (shader == nullptr) {
      continue;
    }
    const auto dir_handle = shader->optional_uniform_handle("u_ground_light_dir");
    const auto cast_handle = shader->optional_uniform_handle("u_cast_weight");
    if (dir_handle == Shader::InvalidUniform && cast_handle == Shader::InvalidUniform) {
      continue;
    }
    shader->use();
    shader->set_uniform(dir_handle, ground);
    shader->set_uniform(cast_handle, cast_weight);
  }
  glUseProgram(0);
}

void Backend::begin_frame() {

  drain_deferred_gl_deletes();
  if (m_terrain_pipeline != nullptr) {
    m_terrain_pipeline->m_frame_ground_cover = {};
  }

  sync_graphics_profile();
  if (m_viewport_width > 0 && m_viewport_height > 0) {
    glViewport(0, 0, m_viewport_width, m_viewport_height);
  }
  if (m_post_process_pipeline != nullptr) {
    (void)m_post_process_pipeline->begin_scene(m_viewport_width, m_viewport_height);
  }
  glClearColor(m_clear_color[red],
               m_clear_color[green],
               m_clear_color[blue],
               m_clear_color[alpha]);

  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindVertexArray(0);
  glUseProgram(0);
  glActiveTexture(GL_TEXTURE0);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  Platform::set_blend_equation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (m_cylinder_pipeline) {
    m_cylinder_pipeline->begin_frame();
  }
  if (m_mesh_instancing_pipeline) {
    m_mesh_instancing_pipeline->begin_frame();
  }
  if (m_rigged_cull_pipeline) {
    m_rigged_cull_pipeline->begin_frame();
  }
}

void Backend::set_viewport(int w, int h) {
  m_viewport_width = w;
  m_viewport_height = h;
}

void Backend::set_clear_color(float r, float g, float b, float a) {
  m_clear_color[red] = r;
  m_clear_color[green] = g;
  m_clear_color[blue] = b;
  m_clear_color[alpha] = a;
}

void Backend::upload_frame_data(const QMatrix4x4& view_proj, bool depth_only) {
  if (!m_frame_ubo) {
    return;
  }
  std::array<float, k_frame_data_bytes / sizeof(float)> block{};
  std::memcpy(block.data(), view_proj.constData(), sizeof(float) * 16U);
  block[16] = depth_only ? 1.0F : 0.0F;
  glBindBuffer(GL_UNIFORM_BUFFER, m_frame_ubo.id());
  glBufferSubData(
      GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(k_frame_data_bytes), block.data());
  note_buffer_transfer(k_frame_data_bytes);
  glBindBufferBase(GL_UNIFORM_BUFFER, k_frame_data_binding_point, m_frame_ubo.id());
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Backend::upload_frame_uniform_buffers(const QMatrix4x4& view_proj,
                                           const DrawQueue& queue,
                                           const Camera& cam) {
  upload_frame_data(view_proj, false);
  if (m_environment_lighting_ubo.valid()) {
    EnvironmentLightingState frame_lighting = m_environment_lighting;
    frame_lighting.cloud_time = m_animation_time;
    const auto packed = frame_lighting.packed_std140();
    glBindBuffer(GL_UNIFORM_BUFFER, m_environment_lighting_ubo.id());
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(packed.size() * sizeof(float)),
                    packed.data());
    note_buffer_transfer(static_cast<std::size_t>(packed.size() * sizeof(float)));
    glBindBufferBase(GL_UNIFORM_BUFFER,
                     k_environment_lighting_binding_point,
                     m_environment_lighting_ubo.id());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
  if (m_local_lighting_ubo.valid()) {
    std::vector<Render::LocalLight> candidates;
    for (const auto& item : queue.items()) {
      const auto* effect = std::get_if<EffectBatchCmd>(&item);
      if (effect == nullptr) {
        continue;
      }
      if (effect->kind != EffectBatchCmd::Kind::BuildingFlame &&
          effect->kind != EffectBatchCmd::Kind::BurningFlame &&
          effect->kind != EffectBatchCmd::Kind::Fireball) {
        continue;
      }
      Render::LocalLight light;
      light.position = effect->position + QVector3D(0.0F, 0.8F, 0.0F);
      light.color =
          effect->color.isNull() ? QVector3D(1.0F, 0.48F, 0.16F) : effect->color;
      light.radius = std::clamp(effect->radius * 5.0F, 3.0F, 12.0F);
      light.intensity = std::clamp(effect->intensity, 0.0F, 2.5F);
      candidates.push_back(light);
    }
    const auto& submitted_lights = queue.local_lights();
    candidates.insert(
        candidates.end(), submitted_lights.begin(), submitted_lights.end());
    const auto selected =
        m_local_light_fader.update(candidates, cam.get_position(), m_animation_time);
    const auto packed = Render::pack_local_lights_std140(selected);

    m_active_local_lights = Render::active_local_lights(selected);
    glBindBuffer(GL_UNIFORM_BUFFER, m_local_lighting_ubo.id());
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(packed.size() * sizeof(float)),
                    packed.data());
    note_buffer_transfer(static_cast<std::size_t>(packed.size() * sizeof(float)));
    glBindBufferBase(
        GL_UNIFORM_BUFFER, k_local_lighting_binding_point, m_local_lighting_ubo.id());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
}

auto Backend::try_collect_frame_slot(FrameGpuTiming& slot) -> bool {
  if (!slot.pending) {
    return true;
  }
  auto const wait_start = std::chrono::steady_clock::now();
  const GLenum polled =
      glClientWaitSync(static_cast<GLsync>(slot.fence.get()), GL_SYNC_FLUSH_COMMANDS_BIT, 0);
  m_last_playback_stats.gpu_wait_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - wait_start)
                                          .count();
  if (polled == GL_TIMEOUT_EXPIRED) {
    ++m_last_playback_stats.gpu_samples_deferred;
    return false;
  }
  if (polled == GL_WAIT_FAILED) {
    qWarning() << "Backend: glClientWaitSync failed while collecting GPU timing; "
                  "dropping this frame's queries";
    slot.fence.reset();
    slot.pending = false;
    slot.mark_count = 0;
    m_last_playback_stats.gpu_shadow_ms = 0.0;
    m_last_playback_stats.gpu_color_ms = 0.0;
    return true;
  }
  collect_frame_slot(slot);
  return true;
}

void Backend::wait_for_frame_slot(FrameGpuTiming& slot) {
  if (!slot.pending) {
    return;
  }
  auto const wait_start = std::chrono::steady_clock::now();
  constexpr GLuint64 k_wait_timeout_ns = 1'000'000'000ULL;
  constexpr int k_max_wait_attempts = 4;
  bool signaled = false;
  for (int attempt = 0; attempt < k_max_wait_attempts && !signaled; ++attempt) {
    const GLenum waited =
        glClientWaitSync(static_cast<GLsync>(slot.fence.get()),
                         GL_SYNC_FLUSH_COMMANDS_BIT,
                         k_wait_timeout_ns);
    if (waited == GL_ALREADY_SIGNALED || waited == GL_CONDITION_SATISFIED) {
      signaled = true;
    } else if (waited == GL_WAIT_FAILED) {
      qWarning() << "Backend: glClientWaitSync failed while collecting GPU timing; "
                    "dropping this frame's queries";
      break;
    }
  }
  m_last_playback_stats.gpu_wait_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - wait_start)
                                          .count();
  if (!signaled) {
    qWarning() << "Backend: GPU timing fence never signaled after"
               << k_max_wait_attempts << "waits; discarding the frame's timing slot";
    slot.fence.reset();
    slot.pending = false;
    slot.mark_count = 0;
    m_last_playback_stats.gpu_shadow_ms = 0.0;
    m_last_playback_stats.gpu_color_ms = 0.0;
    return;
  }
  collect_frame_slot(slot);
}

void Backend::collect_frame_slot(FrameGpuTiming& slot) {
  slot.fence.reset();
  m_last_playback_stats.gpu_sample_frame = slot.frame_index;
  m_last_playback_stats.gpu_sample_age_frames =
      m_frame_timing_index > slot.frame_index ? m_frame_timing_index - slot.frame_index
                                              : 0;
  std::array<GLuint64, 4> ticks{};
  for (std::size_t i = 0; i < ticks.size(); ++i) {
    if (slot.timestamps[i]) {
      glGetQueryObjectui64v(slot.timestamps[i].id(), GL_QUERY_RESULT, &ticks[i]);
    }
  }
  m_last_playback_stats.gpu_shadow_ms =
      static_cast<double>(ticks[1] - ticks[0]) / 1.0e6;
  m_last_playback_stats.gpu_color_ms = static_cast<double>(ticks[2] - ticks[1]) / 1.0e6;
  if (ticks[3] >= ticks[2]) {
    m_gpu_breakdown_post_ms += static_cast<double>(ticks[3] - ticks[2]) / 1.0e6;
  }

  if (m_gpu_breakdown_enabled && slot.mark_count > 0) {
    GLuint64 previous = ticks[1];
    for (std::size_t i = 0; i < slot.mark_count; ++i) {
      GLuint64 tick = 0;
      glGetQueryObjectui64v(slot.marks[i].id(), GL_QUERY_RESULT, &tick);
      const std::uint8_t type = slot.mark_types[i];
      if (type < m_gpu_breakdown_ms.size() && tick >= previous) {
        m_gpu_breakdown_ms[type] += static_cast<double>(tick - previous) / 1.0e6;
      }
      previous = tick;
    }
    ++m_gpu_breakdown_frames;
    if (m_gpu_breakdown_frames >= 120) {
      QString report = QStringLiteral("SOI_GPU_BREAKDOWN per frame:");
      for (std::size_t type = 0; type < m_gpu_breakdown_ms.size(); ++type) {
        if (m_gpu_breakdown_ms[type] <= 0.0) {
          continue;
        }
        report += QStringLiteral(" %1=%2ms")
                      .arg(type)
                      .arg(m_gpu_breakdown_ms[type] /
                               static_cast<double>(m_gpu_breakdown_frames),
                           0,
                           'f',
                           3);
        m_gpu_breakdown_ms[type] = 0.0;
      }
      report += QStringLiteral(" post=%1ms")
                    .arg(m_gpu_breakdown_post_ms /
                             static_cast<double>(m_gpu_breakdown_frames),
                         0,
                         'f',
                         3);
      m_gpu_breakdown_post_ms = 0.0;
      qInfo().noquote() << report;
      m_gpu_breakdown_frames = 0;
    }
  }
  slot.mark_count = 0;
  slot.pending = false;
}

void Backend::gpu_breakdown_mark(FrameGpuTiming& slot, std::uint8_t type) {
  if (!m_gpu_breakdown_enabled || slot.mark_count >= k_gpu_breakdown_marks) {
    return;
  }
  QueryHandle& mark = slot.marks[slot.mark_count];
  if (!mark) {
    mark = QueryHandle::create();
  }
  slot.mark_types[slot.mark_count] = type;
  glQueryCounter(mark.id(), GL_TIMESTAMP);
  ++slot.mark_count;
}

void Backend::execute_scene(const DrawQueue& queue, const Camera& cam) {
  m_last_playback_stats = {};
  m_last_playback_stats.submitted_commands = queue.size();
  m_last_playback_stats.prepared_batches = queue.prepared_batches().size();

  if (m_basic_shader == nullptr) {
    m_frame_tracker.begin_frame();
    m_frame_tracker.mark_complete();
    m_frame_tracker.end_frame();
    return;
  }

  m_frame_timing_active =
      Render::Profiling::global_profile().enabled || m_gpu_breakdown_enabled;

  FrameGpuTiming& frame_timing = m_frame_timings[m_frame_timing_slot];
  m_frame_timing_slot = (m_frame_timing_slot + 1U) % m_frame_timings.size();
  ++m_frame_timing_index;
  const bool timing_requested = m_frame_timing_active;
  const bool slot_ready = try_collect_frame_slot(frame_timing);
  m_frame_timing_active = timing_requested && slot_ready;
  m_frame_tracker.begin_frame();
  if (m_frame_timing_active) {
    frame_timing.frame_index = m_frame_timing_index;
    if (!frame_timing.timestamps[0]) {
      for (QueryHandle& query : frame_timing.timestamps) {
        query = QueryHandle::create();
      }
    }
    glQueryCounter(frame_timing.timestamps[0].id(), GL_TIMESTAMP);
  } else {
    m_active_frame_timing = nullptr;
    if (!timing_requested) {
      m_last_playback_stats.gpu_shadow_ms = 0.0;
      m_last_playback_stats.gpu_color_ms = 0.0;
      m_last_playback_stats.gpu_wait_ms = 0.0;
      m_last_playback_stats.gpu_sample_frame = 0;
      m_last_playback_stats.gpu_sample_age_frames = 0;
    }
  }

  const QMatrix4x4 view = cam.get_view_matrix();
  const QMatrix4x4 projection = cam.get_projection_matrix();
  const QMatrix4x4 view_proj = projection * view;
  m_sky_box_transition.advance(m_animation_time - m_sky_box_blend_time);
  m_sky_box_blend_time = m_animation_time;
  upload_frame_uniform_buffers(view_proj, queue, cam);
  update_contact_shadow_uniforms();
  {
    Render::Profiling::PhaseScope const shadow_scope(
        &Render::Profiling::global_profile(), Render::Profiling::Phase::Shadow);
    render_directional_shadows(queue, cam);
  }
  if (m_frame_timing_active) {
    glQueryCounter(frame_timing.timestamps[1].id(), GL_TIMESTAMP);
  }
  if (m_post_process_pipeline != nullptr && m_post_process_pipeline->is_capturing()) {
    m_post_process_pipeline->set_depth_range(cam.get_near(), cam.get_far());
    const auto [fog_start, fog_end] = fog_range_for_camera(cam);
    m_post_process_pipeline->set_atmosphere(
        view_proj.inverted(), cam.get_position(), fog_start, fog_end, m_animation_time);
    if (m_mist_volumes_dirty) {
      m_post_process_pipeline->set_mist(m_mist_volumes);
      m_mist_volumes_dirty = false;
    }
    m_post_process_pipeline->set_ground_fog(m_ground_fog);
    {
      const QVector3D sun_direction =
          m_environment_lighting.primary_direction.normalized();
      constexpr float k_sun_projection_distance = 4000.0F;
      const QVector4D sun_clip =
          view_proj *
          QVector4D(cam.get_position() + sun_direction * k_sun_projection_distance,
                    1.0F);
      float sun_visibility = 0.0F;
      QVector2D sun_screen(0.5F, 0.5F);
      if (sun_clip.w() > 1e-3F) {
        sun_screen = QVector2D(sun_clip.x() / sun_clip.w() * 0.5F + 0.5F,
                               sun_clip.y() / sun_clip.w() * 0.5F + 0.5F);
        const float outside_x =
            std::max({0.0F, -sun_screen.x(), sun_screen.x() - 1.0F});
        const float outside_y =
            std::max({0.0F, -sun_screen.y(), sun_screen.y() - 1.0F});
        const float outside = std::sqrt(outside_x * outside_x + outside_y * outside_y);
        constexpr float k_sun_offscreen_reach = 0.65F;
        const float frame_weight =
            1.0F - std::clamp(outside / k_sun_offscreen_reach, 0.0F, 1.0F);
        const float low_sun_weight =
            1.0F - std::clamp((sun_direction.y() - 0.15F) / 0.55F, 0.0F, 1.0F);
        const float cloud_weight =
            1.0F - 0.6F * std::clamp(m_environment_lighting.cloud_cover, 0.0F, 1.0F);
        const float intensity_weight =
            std::clamp(m_environment_lighting.primary_intensity / 0.6F, 0.0F, 1.0F);
        sun_visibility = frame_weight * (0.35F + 0.65F * low_sun_weight) *
                         cloud_weight * intensity_weight;
      }
      m_post_process_pipeline->set_sun_screen(sun_screen, sun_visibility);
    }
    m_post_process_pipeline->draw_sky(view_proj, cam.get_position());
    if (m_sky_box_pipeline != nullptr && m_sky_box_transition.is_visible()) {
      m_sky_box_pipeline->draw(
          view, projection, m_animation_time, m_sky_box_transition.blend());
    }
  }
  const float banner_wind_strength = 0.8F + 0.2F * std::sin(m_animation_time * 0.5F);

  m_last_bound_shader = nullptr;
  m_last_bound_texture = nullptr;

  bool polygon_offset_enabled = (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U);

  const auto& prepared_batches = queue.prepared_batches();
  CommandExecutionContext context{queue,
                                  cam,
                                  view,
                                  projection,
                                  view_proj,
                                  banner_wind_strength,
                                  polygon_offset_enabled};

  m_rigged_drawn_this_frame = 0;
  auto& draw_tally = DrawTally::instance();
  draw_tally.reset();

  int breakdown_last_type = -1;
  const auto breakdown_enter = [&](DrawCmdType type) {
    const int index = static_cast<int>(type);
    if (m_gpu_breakdown_enabled && index != breakdown_last_type) {
      if (breakdown_last_type >= 0) {
        gpu_breakdown_mark(frame_timing,
                           static_cast<std::uint8_t>(breakdown_last_type));
      }
      breakdown_last_type = index;
    }
  };

  const StaticBuildingBatch* pending_static_batch =
      supports_static_batch() ? queue.static_batch() : nullptr;
  const auto draw_pending_static_batch = [&]() {
    if (pending_static_batch == nullptr) {
      return;
    }
    breakdown_enter(DrawCmdType::Mesh);
    draw_tally.set_type(static_cast<std::size_t>(DrawCmdType::Mesh));
    execute_static_batch(*pending_static_batch, context);
    pending_static_batch = nullptr;
  };

  draw_pending_static_batch();

  m_opaque_first_batches.assign(prepared_batches.size(), 0U);
  for (std::size_t early = 0; early < prepared_batches.size(); ++early) {
    const PreparedBatch& prepared = prepared_batches[early];
    if (draw_cmd_type(queue.get_sorted(prepared.start)) !=
        DrawCmdType::RiggedCreature) {
      continue;
    }
    breakdown_enter(DrawCmdType::RiggedCreature);
    draw_tally.set_type(static_cast<std::size_t>(DrawCmdType::RiggedCreature));
    execute_rigged_commands(prepared, context);
    for (std::size_t executed = 0; executed < prepared.count; ++executed) {
      m_frame_tracker.record_executed();
    }
    m_opaque_first_batches[early] = 1U;
  }

  std::size_t batch_index = 0;
  while (batch_index < prepared_batches.size()) {
    if (m_opaque_first_batches[batch_index] != 0U) {
      ++batch_index;
      continue;
    }
    const PreparedBatch& prepared = prepared_batches[batch_index];
    const std::size_t i = prepared.start;
    const auto& cmd = queue.get_sorted(i);
    breakdown_enter(draw_cmd_type(cmd));
    draw_tally.set_type(static_cast<std::size_t>(draw_cmd_type(cmd)));
    switch (draw_cmd_type(cmd)) {
    case DrawCmdType::Cylinder:
    case DrawCmdType::FogBatch:
      execute_cylinder_commands(prepared, context);
      break;
    case DrawCmdType::TerrainScatter:
      execute_scatter_commands(prepared, context);
      break;
    case DrawCmdType::TerrainSurface:
      execute_terrain_commands(prepared, context);
      break;
    case DrawCmdType::TerrainFeature:
      execute_water_linear_commands(prepared, context);
      break;
    case DrawCmdType::Mesh:
    case DrawCmdType::DrawPart:
      execute_mesh_commands(prepared, context);
      break;
    case DrawCmdType::RainBatch:
    case DrawCmdType::Grid:
    case DrawCmdType::GroundMarker:
    case DrawCmdType::SelectionSmoke:
    case DrawCmdType::PrimitiveBatch:
    case DrawCmdType::EffectBatch:
    case DrawCmdType::ModeIndicator:
      execute_effects_commands(prepared, context);
      break;
    case DrawCmdType::RiggedCreature:
      execute_rigged_commands(prepared, context);
      break;
    }

    for (std::size_t executed = 0; executed < prepared.count; ++executed) {
      m_frame_tracker.record_executed();
    }
    if (m_frame_budget_config.allow_partial_render &&
        m_frame_tracker.should_defer(m_frame_budget_config)) {
      const std::size_t next_batch = batch_index + 1;
      if (next_batch < prepared_batches.size()) {
        const std::size_t next_i = prepared_batches[next_batch].start;
        auto next_prio = extract_cmd_priority(queue.get_sorted(next_i));
        if (next_prio >= CommandPriority::Low) {

          bool only_low_remaining = true;
          for (std::size_t b = next_batch; b < prepared_batches.size(); ++b) {
            const PreparedBatch& remaining = prepared_batches[b];
            for (std::size_t j = remaining.start; j < remaining.end(); ++j) {
              if (extract_cmd_priority(queue.get_sorted(j)) < CommandPriority::Low) {
                only_low_remaining = false;
                break;
              }
            }
            if (!only_low_remaining) {
              break;
            }
          }
          if (only_low_remaining) {
            for (std::size_t b = next_batch; b < prepared_batches.size(); ++b) {
              const PreparedBatch& remaining = prepared_batches[b];
              for (std::size_t j = remaining.start; j < remaining.end(); ++j) {
                m_frame_tracker.record_deferred();
              }
            }
            break;
          }
        }
      }
    }

    ++batch_index;
  }
  draw_pending_static_batch();
  if (m_last_bound_shader != nullptr) {
    m_last_bound_shader->release();
    m_last_bound_shader = nullptr;
  }

  draw_tally.set_type(DrawTally::k_other);
  m_last_playback_stats.triangles_by_type = draw_tally.triangles;
  m_last_playback_stats.instances_by_type = draw_tally.instances;
  if (m_rigged_cull_pipeline) {
    m_rigged_cull_pipeline->end_frame();
  }
  if (m_cylinder_pipeline) {
    m_cylinder_pipeline->end_frame();
  }
  if (breakdown_last_type >= 0) {
    gpu_breakdown_mark(frame_timing, static_cast<std::uint8_t>(breakdown_last_type));
  }
  if (m_frame_timing_active) {
    glQueryCounter(frame_timing.timestamps[2].id(), GL_TIMESTAMP);
    m_active_frame_timing = &frame_timing;
  }

  m_frame_tracker.mark_complete();
  m_frame_tracker.end_frame();
}

void Backend::execute(const DrawQueue& queue, const Camera& cam) {
  execute_scene(queue, cam);
  if (m_post_process_pipeline != nullptr) {
    m_post_process_pipeline->resolve_scene();
  }
  if (m_active_frame_timing != nullptr) {
    glQueryCounter(m_active_frame_timing->timestamps[3].id(), GL_TIMESTAMP);
    m_active_frame_timing->fence = FenceHandle::insert();
    m_active_frame_timing->pending = m_active_frame_timing->fence.valid();
    m_active_frame_timing = nullptr;
  }
}

} // namespace Render::GL
