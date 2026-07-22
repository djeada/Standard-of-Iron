#include "scene_renderer.h"

#include <QDebug>
#include <qglobal.h>
#include <qvectornd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "../game/map/render_visibility_rules.h"
#include "../game/map/terrain_service.h"
#include "../game/map/visibility_service.h"
#include "../game/systems/combat_rules.h"
#include "../game/systems/nation_registry.h"
#include "../game/systems/owner_registry.h"
#include "../game/systems/troop_profile_service.h"
#include "../game/units/spawn_type.h"
#include "../game/units/troop_catalog.h"
#include "../game/units/troop_config.h"
#include "../game/visuals/team_colors.h"
#include "battle_render_optimizer.h"
#include "creature/archetype_registry.h"
#include "creature/bpat/bpat_registry.h"
#include "creature/pose_intent.h"
#include "creature/quadruped/render_stats.h"
#include "creature/runtime_bake_guard.h"
#include "creature/snapshot_mesh_registry.h"
#include "decoration_gpu.h"
#include "draw_queue.h"
#include "effects_submitter.h"
#include "elephant/dimensions.h"
#include "elephant/elephant_renderer_base.h"
#include "entity/building_render_common.h"
#include "entity/registry.h"
#include "equipment/equipment_registry.h"
#include "equipment/render_archetype_registry.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "geom/mode_indicator.h"
#include "gl/backend.h"
#include "gl/buffer.h"
#include "gl/camera.h"
#include "gl/humanoid/animation/animation_inputs.h"
#include "gl/primitives.h"
#include "gl/resources.h"
#include "graphics_settings.h"
#include "horse/dimensions.h"
#include "horse/horse_renderer_base.h"
#include "humanoid/cache_control.h"
#include "humanoid/humanoid_renderer_base.h"
#include "humanoid/render_stats.h"
#include "pass/construction_preview_pass.h"
#include "pass/frame_context.h"
#include "pass/frame_pass_runner.h"
#include "pass/primitive_flush_pass.h"
#include "pipeline/lod_selector.h"
#include "primitive_batch.h"
#include "profiling/combat_animation_diagnostics.h"
#include "profiling/frame_profile.h"
#include "render_backend_factory.h"
#include "selection_ring_layout.h"
#include "software_backend.h"
#include "submitter.h"
#include "template_cache.h"
#include "template_prewarm_catalog.h"
#include "visibility_budget.h"
#include "world_chunk.h"

namespace Render::GL {

namespace {
constexpr uint32_t k_animation_cache_cleanup_mask = 0x3F;
constexpr uint32_t k_animation_cache_max_age = 240;

#if defined(SOI_ENABLE_RUNTIME_TRACING)
auto render_stage_logging_enabled() -> bool {
  return qEnvironmentVariableIsSet("SOI_RENDER_STAGE_LOG");
}
#endif

#if defined(SOI_ENABLE_RUNTIME_TRACING)
void log_render_first_use_once(const char* stage, const QString& detail) {
  if (!render_stage_logging_enabled()) {
    return;
  }

  static std::mutex mutex;
  static std::unordered_set<std::string> emitted_stages;

  std::lock_guard<std::mutex> const lock(mutex);
  if (!emitted_stages.emplace(stage).second) {
    return;
  }

  qInfo().noquote() << QStringLiteral("SOI render first-use [%1]: %2")
                           .arg(QString::fromLatin1(stage), detail);
}
#endif
} // namespace

Renderer::Renderer(ShaderQuality quality)
    : m_shader_quality(quality)
    , m_effects_submitter(std::make_unique<EffectsSubmitter>()) {
  m_active_queue = &m_queues[m_fill_queue_index];
}

Renderer::~Renderer() {
  shutdown();
}

void Renderer::set_world_render_mode(WorldRenderMode mode) {
  if (m_world_render_mode == mode) {
    return;
  }
  m_world_render_mode = mode;
}

auto Renderer::visibility_mode_config() const -> VisibilityModeConfig {
  switch (m_world_render_mode) {
  case WorldRenderMode::Rts:
    return {.filter_non_local_units = true, .filter_static_world = true};
  case WorldRenderMode::Rpg:
    return {.filter_non_local_units = true, .filter_static_world = false};
  }
  return {};
}

auto Renderer::non_local_unit_visibility_filter_enabled() const -> bool {
  return visibility_mode_config().filter_non_local_units;
}

auto Renderer::static_world_visibility_filter_enabled() const -> bool {
  return visibility_mode_config().filter_static_world;
}

auto Renderer::initialize() -> bool {
  Render::Creature::set_runtime_bake_forbidden(false);
  if (!m_backend) {
    m_backend = RenderBackendFactory::create(m_shader_quality);
    m_gl_backend = dynamic_cast<Backend*>(m_backend.get());
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    log_render_first_use_once(
        "backend-create",
        QStringLiteral("created render backend for QSG/FBO render thread"));
#endif
  }
  if (!m_backend->initialize()) {
    qCritical() << "Renderer::initialize() - backend initialization failed;"
                   " rendering is disabled.";
    return false;
  }
  m_entity_registry = std::make_unique<EntityRendererRegistry>();
  register_built_in_entity_renderers(*m_entity_registry);
  register_built_in_equipment();
  const auto warmed_archetypes = RenderArchetypeRegistry::instance().warm_all();
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  log_render_first_use_once(
      "renderer-registries",
      QStringLiteral("registered entity renderers and equipment renderers; "
                     "warmed %1 render archetypes")
          .arg(static_cast<qulonglong>(warmed_archetypes)));
#else
  (void)warmed_archetypes;
#endif

  const std::size_t loaded_bpat =
      Render::Creature::Bpat::BpatRegistry::instance().load_all("assets/creatures");
  if (loaded_bpat != Render::Creature::Bpat::k_species_count) {
    qWarning() << "Renderer: loaded" << loaded_bpat << "of"
               << Render::Creature::Bpat::k_species_count << "BPAT creature assets:"
               << QString::fromStdString(std::string(
                      Render::Creature::Bpat::BpatRegistry::instance().last_error()));
  }
  (void)Render::Creature::Snapshot::SnapshotMeshRegistry::instance().load_all(
      "assets/creatures");
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  log_render_first_use_once(
      "creature-assets",
      QStringLiteral("loaded BPAT and snapshot creature asset registries"));
#endif
  return true;
}

void Renderer::shutdown() {
  cancel_async_template_prewarm();
  Render::Creature::set_runtime_bake_forbidden(false);
  m_gl_backend = nullptr;
  m_backend.reset();
}

void Renderer::begin_frame() {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  auto& profile = Render::Profiling::global_profile();
  profile.reset();
  profile.frame_index += 1;
  Render::Profiling::CombatAnimationDiagnostics::instance().begin_frame(
      profile.frame_index);
  Render::Profiling::PhaseScope const collect_scope(
      &profile, Render::Profiling::Phase::Collection);
#endif

  advance_pose_cache_frame();

  reset_humanoid_render_stats();
  reset_horse_render_stats();
  reset_elephant_render_stats();

  Render::VisibilityBudgetTracker::instance().reset_frame();
  auto& battle_optimizer = Render::BattleRenderOptimizer::instance();
  battle_optimizer.begin_frame();
  prune_animation_time_cache(battle_optimizer.frame_counter());

  m_active_queue = &m_queues[m_fill_queue_index];
  m_active_queue->clear();
  m_active_queue->reserve_for_frame();

  if (m_camera != nullptr) {
    m_view_proj = m_camera->get_view_projection_matrix();
  }
  auto& visibility = Game::Map::VisibilityService::instance();
  m_frame_visibility_snapshot =
      visibility.is_initialized() ? visibility.snapshot_ptr() : nullptr;
  m_submission_visibility.reset(m_camera, m_frame_visibility_snapshot.get());

  if (m_backend) {
    m_backend->begin_frame();
  }

  process_async_template_prewarm();
  m_rigged_mesh_cache.upload_pending_skin_ubos();
}

void Renderer::end_frame() {
  if (m_paused.load()) {
    return;
  }
  if (m_backend && (m_camera != nullptr)) {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    auto& profile = Render::Profiling::global_profile();
#endif
    std::swap(m_fill_queue_index, m_render_queue_index);
    DrawQueue& render_queue = m_queues[m_render_queue_index];
    {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
      Render::Profiling::PhaseScope const sort_scope(&profile,
                                                     Render::Profiling::Phase::Sort);
#endif
      render_queue.sort_for_batching();
    }
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    profile.draw_calls = static_cast<std::uint64_t>(render_queue.size());
#endif
    m_backend->set_animation_time(m_accumulated_time);
    {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
      Render::Profiling::PhaseScope const play_scope(
          &profile, Render::Profiling::Phase::Playback);
#endif

      m_backend->execute(render_queue, *m_camera);
    }
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    constexpr double k_frame_budget_ms = 16.67;
    profile.budget_headroom_ms =
        k_frame_budget_ms - static_cast<double>(profile.total_us()) / 1000.0;
    profile.finish_frame_sample();
#endif
  }
}

void Renderer::set_camera(Camera* camera) {
  m_camera = camera;
  m_submission_visibility.reset(m_camera, m_frame_visibility_snapshot.get());
}

auto Renderer::render_software_preview(int width, int height) -> QImage {
  if (m_camera == nullptr || width <= 0 || height <= 0) {
    return {};
  }
  DrawQueue const& queue = m_queues[m_render_queue_index];
  SoftwareBackend backend;
  Render::Software::RasterSettings settings;
  settings.width = width;
  settings.height = height;
  backend.set_settings(settings);
  backend.begin_frame();
  backend.execute(queue, *m_camera);
  return backend.last_frame().copy();
}

void Renderer::set_clear_color(float r, float g, float b, float a) {
  if (m_backend) {
    m_backend->set_clear_color(r, g, b, a);
  }
}

void Renderer::set_viewport(int width, int height) {
  m_viewport_width = width;
  m_viewport_height = height;
  if (m_backend) {
    m_backend->set_viewport(width, height);
  }
  if ((m_camera != nullptr) && height > 0) {
    float const aspect = float(width) / float(height);
    m_camera->set_perspective(
        m_camera->get_fov(), aspect, m_camera->get_near(), m_camera->get_far());
  }
}

auto Renderer::resolve_animation_time(uint32_t entity_id,
                                      bool update,
                                      float current_time,
                                      uint32_t frame) -> float {
  if (entity_id == 0U) {
    return current_time;
  }

  auto& entry = m_animation_time_cache[entity_id];
  if (update || entry.last_frame == 0U) {
    entry.time = current_time;
  }
  entry.last_frame = frame;
  return entry.time;
}

void Renderer::prune_animation_time_cache(uint32_t frame) {
  if ((frame & k_animation_cache_cleanup_mask) != 0U) {
    return;
  }

  for (auto it = m_animation_time_cache.begin(); it != m_animation_time_cache.end();) {
    if (frame - it->second.last_frame > k_animation_cache_max_age) {
      it = m_animation_time_cache.erase(it);
    } else {
      ++it;
    }
  }
}

void Renderer::mesh(Mesh* mesh,
                    const QMatrix4x4& model,
                    const QVector3D& color,
                    Texture* texture,
                    float alpha,
                    int material_id) {
  if (mesh == nullptr) {
    return;
  }

  float const effective_alpha = alpha * m_alpha_override;

  static Mesh* const unit_cylinder_mesh = get_unit_cylinder();
  if (mesh == unit_cylinder_mesh && (texture == nullptr) &&
      (m_current_shader == nullptr)) {
    QVector3D start;
    QVector3D end;
    float radius = 0.0F;
    if (detail::decompose_unit_cylinder(model, start, end, radius)) {
      cylinder(start, end, radius, color, effective_alpha);
      return;
    }
  }
  MeshCmd cmd;
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.model = model;
  cmd.color = color;
  cmd.alpha = effective_alpha;
  cmd.material_id = material_id;
  cmd.shader = m_current_shader;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::banner(Mesh* mesh,
                      const QMatrix4x4& model,
                      const QVector3D& color,
                      const QVector3D& trim_color,
                      Texture* texture,
                      float alpha,
                      int material_id) {
  if (mesh == nullptr) {
    return;
  }

  MeshCmd cmd;
  cmd.mesh = mesh;
  cmd.texture = texture;
  cmd.model = model;
  cmd.color = color;
  cmd.trim_color = trim_color;
  cmd.has_trim_color = true;
  cmd.alpha = alpha * m_alpha_override;
  cmd.material_id = material_id;
  cmd.shader = m_current_shader;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::part(Mesh* mesh,
                    Material* material,
                    const QMatrix4x4& model,
                    const QVector3D& color,
                    Texture* texture,
                    float alpha,
                    int material_id) {
  if (mesh == nullptr) {
    return;
  }
  if (material == nullptr) {

    this->mesh(mesh, model, color, texture, alpha, material_id);
    return;
  }
  float const effective_alpha = alpha * m_alpha_override;
  DrawPartCmd cmd;
  cmd.mesh = mesh;
  cmd.material = material;
  cmd.world = model;
  cmd.color = color;
  cmd.alpha = effective_alpha;
  cmd.texture = texture;
  cmd.material_id = material_id;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::cylinder(const QVector3D& start,
                        const QVector3D& end,
                        float radius,
                        const QVector3D& color,
                        float alpha) {

  float const effective_alpha = alpha * m_alpha_override;
  CylinderCmd cmd;
  cmd.start = start;
  cmd.end = end;
  cmd.radius = radius;
  cmd.color = color;
  cmd.alpha = effective_alpha;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::fog_batch(const FogInstanceData* instances, std::size_t count) {
  if ((instances == nullptr) || count == 0 || (m_active_queue == nullptr)) {
    return;
  }
  FogBatchCmd cmd;
  cmd.instances = instances;
  cmd.count = count;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::fog_batch(Buffer* instance_buffer, std::size_t count) {
  if ((instance_buffer == nullptr) || count == 0 || (m_active_queue == nullptr)) {
    return;
  }
  FogBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.count = count;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::rain_batch(Buffer* instance_buffer,
                          std::size_t instance_count,
                          const RainBatchParams& params) {
  if ((instance_buffer == nullptr) || instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  RainBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulated_time;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::terrain_surface(const TerrainSurfaceCmd& cmd) {
  if ((cmd.mesh == nullptr) || (m_active_queue == nullptr)) {
    return;
  }
  m_active_queue->submit(cmd);
}

void Renderer::terrain_feature(const TerrainFeatureCmd& cmd) {
  if ((cmd.mesh == nullptr) || (m_active_queue == nullptr)) {
    return;
  }
  const QVector3D world_center = cmd.model.map(cmd.mesh->bounds_center());
  const QVector3D scale_x(cmd.model(0, 0), cmd.model(1, 0), cmd.model(2, 0));
  const QVector3D scale_y(cmd.model(0, 1), cmd.model(1, 1), cmd.model(2, 1));
  const QVector3D scale_z(cmd.model(0, 2), cmd.model(1, 2), cmd.model(2, 2));
  const float max_scale =
      std::max({scale_x.length(), scale_y.length(), scale_z.length()});
  if (!m_submission_visibility.accepts_sphere(
          world_center,
          cmd.mesh->bounds_radius() * max_scale,
          SubmissionFogMode::Ignore)) {
    return;
  }
  TerrainFeatureCmd submitted = cmd;
  submitted.alpha *= m_alpha_override;
  m_active_queue->submit(std::move(submitted));
}

void Renderer::terrain_scatter(const TerrainScatterCmd& cmd) {
  if ((cmd.instance_buffer == nullptr) || cmd.instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  TerrainScatterCmd submitted = cmd;
  switch (submitted.species) {
  case TerrainScatterCmd::Species::Grass:
    submitted.grass.time = m_accumulated_time;
    break;
  case TerrainScatterCmd::Species::Plant:
    submitted.plant.time = m_accumulated_time;
    break;
  case TerrainScatterCmd::Species::Pine:
    submitted.pine.time = m_accumulated_time;
    break;
  case TerrainScatterCmd::Species::Olive:
    submitted.olive.time = m_accumulated_time;
    break;
  case TerrainScatterCmd::Species::FireCamp:
    submitted.firecamp.time = m_accumulated_time;
    break;
  case TerrainScatterCmd::Species::Stone:
    break;
  }
  m_active_queue->submit(std::move(submitted));
}

void Renderer::selection_ring(const QMatrix4x4& model,
                              float alpha_inner,
                              float alpha_outer,
                              const QVector3D& color) {
  SelectionRingCmd cmd;
  cmd.model = model;
  cmd.alpha_inner = alpha_inner;
  cmd.alpha_outer = alpha_outer;
  cmd.color = color;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::grid(const QMatrix4x4& model,
                    const QVector3D& color,
                    float cell_size,
                    float thickness,
                    float extent) {
  GridCmd cmd;
  cmd.model = model;
  cmd.mvp = m_view_proj * model;
  cmd.color = color;
  cmd.cell_size = cell_size;
  cmd.thickness = thickness;
  cmd.extent = extent;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::selection_smoke(const QMatrix4x4& model,
                               const QVector3D& color,
                               float base_alpha) {
  SelectionSmokeCmd cmd;
  cmd.model = model;
  cmd.color = color;
  cmd.base_alpha = base_alpha;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::mode_indicator(const QMatrix4x4& model,
                              int mode_type,
                              const QVector3D& color,
                              float alpha) {
  ModeIndicatorCmd cmd;
  cmd.model = model;
  cmd.mode_type = mode_type;
  cmd.color = color;
  cmd.alpha = alpha;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::rigged(const RiggedCreatureCmd& cmd) {
  if (m_active_queue == nullptr || cmd.mesh == nullptr) {
    return;
  }
  RiggedCreatureCmd submitted = cmd;
  submitted.alpha *= m_alpha_override;
  m_active_queue->submit(std::move(submitted));
}

} // namespace Render::GL
