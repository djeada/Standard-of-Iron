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
const QVector3D k_axis_x(1.0F, 0.0F, 0.0F);
const QVector3D k_axis_y(0.0F, 1.0F, 0.0F);
const QVector3D k_axis_z(0.0F, 0.0F, 1.0F);
constexpr uint32_t k_animation_cache_cleanup_mask = 0x3F;
constexpr uint32_t k_animation_cache_max_age = 240;

auto prewarm_seed_for_variant(int owner_id,
                              std::uint8_t variant) noexcept -> std::uint32_t {
  std::uint32_t seed = static_cast<std::uint32_t>(owner_id) * 2654435761U;
  seed ^= (static_cast<std::uint32_t>(variant) + 1U) * 2246822519U;
  seed ^= seed >> 15U;
  seed *= 2246822519U;
  seed ^= seed >> 13U;
  seed *= 3266489917U;
  seed ^= seed >> 16U;
  return seed;
}

auto prewarm_entity_id_for_variant(std::size_t profile_index,
                                   int owner_id,
                                   std::uint8_t lod,
                                   std::uint8_t variant) noexcept -> std::uint32_t {
  return 1U + static_cast<std::uint32_t>(
                  (((profile_index * 16U) + static_cast<std::size_t>(owner_id)) * 4U +
                   static_cast<std::size_t>(lod)) *
                      k_template_variant_count +
                  static_cast<std::size_t>(variant));
}

void populate_template_prewarm_entity(Engine::Core::Entity& entity,
                                      Game::Units::SpawnType spawn_type,
                                      Game::Systems::NationID nation_id,
                                      int owner_id,
                                      int max_health,
                                      const std::string& renderer_id) {
  auto* unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = spawn_type;
  unit->owner_id = owner_id;
  unit->nation_id = nation_id;
  unit->max_health = std::max(1, max_health);
  unit->health = unit->max_health;

  auto* transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>("", "");
  renderable->renderer_id = renderer_id;
  renderable->visible = true;
  QVector3D const team_color = Game::Visuals::team_colorForOwner(owner_id);
  renderable->color[0] = team_color.x();
  renderable->color[1] = team_color.y();
  renderable->color[2] = team_color.z();
}

auto make_template_prewarm_draw_context(Renderer& renderer,
                                        Engine::Core::Entity& entity,
                                        const std::string& renderer_id,
                                        int owner_id,
                                        HumanoidLOD lod,
                                        std::uint8_t variant,
                                        bool allow_template_cache,
                                        bool force_horse_lod,
                                        const AnimationInputs* animation_override,
                                        std::uint8_t attack_variant_override,
                                        bool has_attack_variant_override)
    -> DrawContext {
  DrawContext ctx{renderer.resources(), &entity, nullptr, QMatrix4x4()};
  ctx.renderer_id = renderer_id;
  ctx.backend = renderer.backend();
  ctx.camera = nullptr;
  ctx.allow_template_cache = allow_template_cache;
  ctx.template_prewarm = true;
  ctx.has_seed_override = true;
  ctx.seed_override = prewarm_seed_for_variant(owner_id, variant);
  ctx.has_variant_override = true;
  ctx.variant_override = variant;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = lod;
  ctx.force_horse_lod = force_horse_lod;
  if (ctx.force_horse_lod) {
    ctx.forced_horse_lod = static_cast<HorseLOD>(lod);
  }
  ctx.animation_override = animation_override;
  ctx.has_attack_variant_override = has_attack_variant_override;
  ctx.attack_variant_override = attack_variant_override;
  return ctx;
}

void execute_template_prewarm_item(Renderer& renderer,
                                   std::size_t profile_index,
                                   Game::Units::SpawnType spawn_type,
                                   Game::Systems::NationID nation_id,
                                   int max_health,
                                   const std::string& renderer_id,
                                   bool is_mounted,
                                   bool is_elephant,
                                   const RenderFunc& fn,
                                   int owner_id,
                                   HumanoidLOD lod,
                                   std::uint8_t variant,
                                   bool allow_template_cache,
                                   const AnimKey& anim_key);

auto render_stage_logging_enabled() -> bool {
  return qEnvironmentVariableIsSet("SOI_RENDER_STAGE_LOG");
}

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

float get_unit_cull_radius(Game::Units::SpawnType spawn_type) {
  switch (spawn_type) {
  case Game::Units::SpawnType::MountedKnight:
    return 4.0F;
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::Knight:
    return 2.5F;
  default:
    return 3.0F;
  }
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit_comp)
    -> int {
  if (unit_comp.render_individuals_per_unit_override > 0) {
    return unit_comp.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(
      unit_comp.spawn_type);
}

auto is_unit_combat_active(const Engine::Core::Entity* entity,
                           const Engine::Core::AttackComponent* attack_comp,
                           const Engine::Core::AttackTargetComponent* attack_target,
                           const Engine::Core::CombatStateComponent* combat_state,
                           const Engine::Core::HitFeedbackComponent* hit_feedback)
    -> bool {
  if ((hit_feedback != nullptr) && hit_feedback->is_reacting) {
    return true;
  }

  if ((combat_state != nullptr) &&
      (combat_state->animation_state != Engine::Core::CombatAnimationState::Idle)) {
    return true;
  }

  if (attack_comp == nullptr) {
    return false;
  }

  if (attack_comp->in_melee_lock &&
      Game::Systems::CombatRules::participates_in_rts_melee_lock(entity)) {
    return true;
  }

  if ((attack_target == nullptr) || (attack_target->target_id == 0)) {
    return false;
  }

  return attack_comp->time_since_last < attack_comp->get_current_cooldown();
}

auto stable_combat_creature_lod(const Engine::Core::UnitComponent* unit,
                                float distance_sq) noexcept -> HumanoidLOD {
  const auto& settings = Render::GraphicsSettings::instance();
  float full_distance = settings.humanoid_full_detail_distance();

  if (unit != nullptr) {
    using Game::Units::SpawnType;
    switch (unit->spawn_type) {
    case SpawnType::HorseArcher:
    case SpawnType::HorseSpearman:
    case SpawnType::MountedKnight:
      full_distance = settings.horse_full_detail_distance();
      break;
    case SpawnType::Elephant:
      full_distance = settings.elephant_full_detail_distance();
      break;
    default:
      break;
    }
  }

  return distance_sq <= full_distance * full_distance ? HumanoidLOD::Full
                                                      : HumanoidLOD::Minimal;
}

struct UnitRenderEntry {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  Engine::Core::MotionPresentationComponent* motion{nullptr};
  std::string renderer_key;
  Render::GL::RendererHandle renderer_handle{Render::GL::k_invalid_renderer_handle};
  QMatrix4x4 model_matrix;
  uint32_t entity_id{0};
  bool selected{false};
  bool hovered{false};
  bool combat_active{false};
  bool in_frustum{true};
  bool fog_visible{true};
  bool has_attack{false};
  bool has_guard_mode{false};
  bool has_hold_mode{false};
  bool has_patrol{false};
  float distance_sq{0.0F};
};

} // namespace

namespace {

struct RenderEntry {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  std::string renderer_key;
  Render::GL::RendererHandle renderer_handle{Render::GL::k_invalid_renderer_handle};
  uint32_t entity_id{0};
  bool selected{false};
  bool hovered{false};
};

class CreatureCacheWarmupSubmitter final : public BatchingSubmitter {
public:
  explicit CreatureCacheWarmupSubmitter(Renderer* renderer)
      : BatchingSubmitter(renderer, nullptr) {}

  void mesh(Mesh*,
            const QMatrix4x4&,
            const QVector3D&,
            Texture* = nullptr,
            float = 1.0F,
            int = 0) override {}
  void banner(Mesh*,
              const QMatrix4x4&,
              const QVector3D&,
              const QVector3D&,
              Texture* = nullptr,
              float = 1.0F,
              int = 0) override {}
  void cylinder(const QVector3D&,
                const QVector3D&,
                float,
                const QVector3D&,
                float = 1.0F) override {}
  void selection_ring(const QMatrix4x4&, float, float, const QVector3D&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float = 0.15F) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float = 1.0F) override {
  }
  void rigged(const RiggedCreatureCmd&) override {}
};

void execute_template_prewarm_item(Renderer& renderer,
                                   std::size_t profile_index,
                                   Game::Units::SpawnType spawn_type,
                                   Game::Systems::NationID nation_id,
                                   int max_health,
                                   const std::string& renderer_id,
                                   bool is_mounted,
                                   bool is_elephant,
                                   const RenderFunc& fn,
                                   int owner_id,
                                   HumanoidLOD lod,
                                   std::uint8_t variant,
                                   bool allow_template_cache,
                                   const AnimKey& anim_key) {
  if (!fn) {
    return;
  }

  Engine::Core::Entity entity(prewarm_entity_id_for_variant(
      profile_index, owner_id, static_cast<std::uint8_t>(lod), variant));
  populate_template_prewarm_entity(
      entity, spawn_type, nation_id, owner_id, max_health, renderer_id);

  AnimationInputs const anim = make_animation_inputs(anim_key);
  bool const attack_state = Render::Creature::is_attack_pose_intent(anim_key.state);
  DrawContext ctx = make_template_prewarm_draw_context(renderer,
                                                       entity,
                                                       renderer_id,
                                                       owner_id,
                                                       lod,
                                                       variant,
                                                       allow_template_cache,
                                                       is_mounted || is_elephant,
                                                       &anim,
                                                       anim_key.attack_variant,
                                                       attack_state);

  CreatureCacheWarmupSubmitter warmup_submitter(&renderer);
  fn(ctx, warmup_submitter);
}
} // namespace

Renderer::Renderer(ShaderQuality quality)
    : m_shader_quality(quality) {
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
    log_render_first_use_once(
        "backend-create",
        QStringLiteral("created render backend for QSG/FBO render thread"));
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
  log_render_first_use_once(
      "renderer-registries",
      QStringLiteral("registered entity renderers and equipment renderers; "
                     "warmed %1 render archetypes")
          .arg(static_cast<qulonglong>(warmed_archetypes)));

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
  log_render_first_use_once(
      "creature-assets",
      QStringLiteral("loaded BPAT and snapshot creature asset registries"));
  return true;
}

void Renderer::shutdown() {
  cancel_async_template_prewarm();
  Render::Creature::set_runtime_bake_forbidden(false);
  m_gl_backend = nullptr;
  m_backend.reset();
}

void Renderer::begin_frame() {

  auto& profile = Render::Profiling::global_profile();
  profile.reset();
  profile.frame_index += 1;
  Render::Profiling::PhaseScope const collect_scope(
      &profile, Render::Profiling::Phase::Collection);

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
    m_view_proj = m_camera->get_projection_matrix() * m_camera->get_view_matrix();
  }

  if (m_backend) {
    m_backend->begin_frame();
  }

  process_async_template_prewarm();
}

void Renderer::end_frame() {
  if (m_paused.load()) {
    return;
  }
  if (m_backend && (m_camera != nullptr)) {
    auto& profile = Render::Profiling::global_profile();
    std::swap(m_fill_queue_index, m_render_queue_index);
    DrawQueue& render_queue = m_queues[m_render_queue_index];
    {
      Render::Profiling::PhaseScope const sort_scope(&profile,
                                                     Render::Profiling::Phase::Sort);
      render_queue.sort_for_batching();
    }
    profile.draw_calls = static_cast<std::uint64_t>(render_queue.size());
    m_backend->set_animation_time(m_accumulated_time);
    {
      Render::Profiling::PhaseScope const play_scope(
          &profile, Render::Profiling::Phase::Playback);

      m_backend->execute(render_queue, *m_camera);
    }
    constexpr double k_frame_budget_ms = 16.67;
    profile.budget_headroom_ms =
        k_frame_budget_ms - static_cast<double>(profile.total_us()) / 1000.0;
  }
}

void Renderer::set_camera(Camera* camera) {
  m_camera = camera;
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

void Renderer::cancel_async_template_prewarm() {
  std::shared_ptr<AsyncTemplatePrewarmState> state;
  {
    std::lock_guard<std::mutex> const lock(m_async_prewarm_mutex);
    state = std::move(m_async_prewarm_state);
  }
  if (state) {
    state->cancel_requested.store(true, std::memory_order_relaxed);
  }
}

void Renderer::run_template_prewarm_item(const PrewarmProfile& profile,
                                         const PrewarmWorkItem& item) {
  execute_template_prewarm_item(*this,
                                item.profile_index,
                                profile.spawn_type,
                                profile.nation_id,
                                profile.max_health,
                                profile.renderer_id,
                                profile.is_mounted,
                                profile.is_elephant,
                                profile.fn,
                                item.owner_id,
                                item.lod,
                                item.variant,
                                true,
                                item.anim_key);
}

void Renderer::process_async_template_prewarm() {
  std::shared_ptr<AsyncTemplatePrewarmState> state;
  {
    std::lock_guard<std::mutex> const lock(m_async_prewarm_mutex);
    state = m_async_prewarm_state;
  }
  if (!state || state->cancel_requested.load(std::memory_order_relaxed)) {
    return;
  }

  using Render::GraphicsQuality;
  std::size_t max_items = 160;
  std::chrono::microseconds time_budget(2000);
  switch (Render::GraphicsSettings::instance().quality()) {
  case GraphicsQuality::Low:
    max_items = 96;
    time_budget = std::chrono::microseconds(1200);
    break;
  case GraphicsQuality::Medium:
    max_items = 160;
    time_budget = std::chrono::microseconds(2000);
    break;
  case GraphicsQuality::High:
    max_items = 240;
    time_budget = std::chrono::microseconds(3000);
    break;
  case GraphicsQuality::Ultra:
  default:
    max_items = 320;
    time_budget = std::chrono::microseconds(4000);
    break;
  }

  const auto& battle_optimizer = Render::BattleRenderOptimizer::instance();
  const int visible_units = battle_optimizer.visible_unit_count();
  if (visible_units >= 300) {
    if ((battle_optimizer.frame_counter() & 1U) != 0U) {
      return;
    }
    max_items = std::min<std::size_t>(max_items, 12);
    time_budget = std::min(time_budget, std::chrono::microseconds(300));
  } else if (visible_units >= 220) {
    max_items = std::min<std::size_t>(max_items, 24);
    time_budget = std::min(time_budget, std::chrono::microseconds(600));
  } else if (visible_units >= 150) {
    max_items = std::min<std::size_t>(max_items, 48);
    time_budget = std::min(time_budget, std::chrono::microseconds(1000));
  }

  std::size_t processed = 0;
  const auto start_time = std::chrono::steady_clock::now();
  while (!state->cancel_requested.load(std::memory_order_relaxed) &&
         processed < max_items) {
    const std::size_t idx = state->next_index.fetch_add(1, std::memory_order_relaxed);
    if (idx >= state->work_items.size()) {
      break;
    }

    const auto& item = state->work_items[idx];
    if (item.profile_index < state->profiles.size()) {
      run_template_prewarm_item(state->profiles[item.profile_index], item);
    }
    ++processed;

    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed >= time_budget) {
      break;
    }
  }

  if (state->cancel_requested.load(std::memory_order_relaxed) ||
      (state->next_index.load(std::memory_order_relaxed) >= state->work_items.size())) {
    std::lock_guard<std::mutex> const lock(m_async_prewarm_mutex);
    if (m_async_prewarm_state == state) {
      m_async_prewarm_state.reset();
      if (m_forbid_runtime_bake_when_async_prewarm_done) {
        Render::Creature::set_runtime_bake_forbidden(true);
        m_forbid_runtime_bake_when_async_prewarm_done = false;
      }
    }
  }
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

void Renderer::healing_beam(const QVector3D& start,
                            const QVector3D& end,
                            const QVector3D& color,
                            float progress,
                            float beam_width,
                            float intensity,
                            float time) {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::HealingBeam;
  cmd.position = start;
  cmd.end_pos = end;
  cmd.color = color;
  cmd.progress = progress;
  cmd.beam_width = beam_width;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::High;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::healer_aura(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time) {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::HealerAura;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::Normal;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::combat_dust(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time) {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::CombatDust;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::Low;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::building_flame(const QVector3D& position,
                              const QVector3D& color,
                              float radius,
                              float intensity,
                              float time) {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::BuildingFlame;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::Normal;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::blood_pool(const QVector3D& position,
                          float radius,
                          float alpha_scale,
                          float rotation,
                          float aspect_ratio,
                          float seed) {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::BloodPool;
  cmd.position = position;
  cmd.radius = radius;
  cmd.alpha_scale = alpha_scale;
  cmd.rotation = rotation;
  cmd.aspect_ratio = aspect_ratio;
  cmd.seed = seed;
  cmd.priority = CommandPriority::Low;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::stone_impact(const QVector3D& position,
                            const QVector3D& color,
                            float radius,
                            float intensity,
                            float time) {
  EffectBatchCmd cmd;
  cmd.kind = EffectBatchCmd::Kind::StoneImpact;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  cmd.priority = CommandPriority::High;
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

void Renderer::enqueue_selection_ring(Engine::Core::Entity* entity,
                                      Engine::Core::TransformComponent* transform,
                                      Engine::Core::UnitComponent* unit_comp,
                                      bool selected,
                                      bool hovered) {
  if ((!selected && !hovered) || (transform == nullptr)) {
    return;
  }

  float ring_size = 0.5F;
  float const ring_offset = 0.05F;
  float ground_offset = 0.0F;
  float scale_y = 1.0F;
  std::vector<SelectionRingPlacement> placements;

  if (unit_comp != nullptr) {
    auto troop_type_opt = Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);
    auto& config = Game::Units::TroopConfig::instance();

    if (troop_type_opt) {
      const auto& nation_reg = Game::Systems::NationRegistry::instance();
      const Game::Systems::Nation* nation =
          nation_reg.get_nation_for_player(unit_comp->owner_id);
      Game::Systems::NationID const nation_id =
          nation != nullptr ? nation->id : nation_reg.default_nation_id();

      const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          nation_id, *troop_type_opt);
      int const individuals_per_unit = resolved_individuals_per_unit(*unit_comp);
      int const max_units_per_row = config.get_max_units_per_row(unit_comp->spawn_type);
      std::uint32_t layout_seed =
          static_cast<std::uint32_t>(unit_comp->owner_id) * 2654435761U;
      if (entity != nullptr) {
        layout_seed ^= static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(entity) & 0xFFFFFFFFU);
      }
      float const formation_spacing = resolve_formation_spacing(
          unit_comp->spawn_type, profile.visuals.formation_spacing);

      ring_size =
          Detail::selection_ring_visual_size(unit_comp->spawn_type,
                                             individuals_per_unit,
                                             profile.visuals.selection_ring_size,
                                             formation_spacing);
      ground_offset = profile.visuals.selection_ring_ground_offset;

      bool is_builder_constructing = false;
      if (entity != nullptr &&
          unit_comp->spawn_type == Game::Units::SpawnType::Builder) {
        auto* builder_prod =
            entity->get_component<Engine::Core::BuilderProductionComponent>();
        if (builder_prod != nullptr && builder_prod->in_progress &&
            builder_prod->at_construction_site) {
          is_builder_constructing = true;
        }
      }

      placements = build_selection_ring_layout(
          {.spawn_type = unit_comp->spawn_type,
           .nation_id = unit_comp->nation_id,
           .individuals_per_unit = individuals_per_unit,
           .max_units_per_row = max_units_per_row,
           .health_ratio =
               unit_comp->max_health > 0
                   ? std::clamp(
                         unit_comp->health / float(unit_comp->max_health), 0.0F, 1.0F)
                   : 1.0F,
           .ring_size = ring_size,
           .formation_spacing = formation_spacing,
           .seed = layout_seed,
           .position = QVector3D(
               transform->position.x, transform->position.y, transform->position.z),
           .rotation = QVector3D(
               transform->rotation.x, transform->rotation.y, transform->rotation.z),
           .scale =
               QVector3D(transform->scale.x, transform->scale.y, transform->scale.z),
           .is_builder_constructing = is_builder_constructing});
    } else {

      ring_size = config.get_selection_ring_size(unit_comp->spawn_type);
      ground_offset = config.get_selection_ring_ground_offset(unit_comp->spawn_type);
    }
  }
  if (transform != nullptr) {
    scale_y = transform->scale.y;
  }

  if (placements.empty()) {
    placements.push_back({transform->position.x, transform->position.z, ring_size});
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
  for (const SelectionRingPlacement& placement : placements) {
    QVector3D const grounded_center = terrain_service.resolve_surface_world_position(
        placement.world_x,
        placement.world_z,
        0.0F,
        transform->position.y - ground_offset * scale_y);

    QMatrix4x4 ring_model;
    ring_model.translate(
        grounded_center.x(), grounded_center.y() + ring_offset, grounded_center.z());
    ring_model.scale(placement.ring_size, 1.0F, placement.ring_size);

    if (selected) {
      selection_ring(ring_model, 0.6F, 0.25F, QVector3D(0.2F, 0.4F, 1.0F));
    } else if (hovered) {
      selection_ring(ring_model, 0.35F, 0.15F, QVector3D(0.90F, 0.90F, 0.25F));
    }
  }
}

void Renderer::enqueue_mode_indicator(Engine::Core::TransformComponent* transform,
                                      Engine::Core::UnitComponent* unit_comp,
                                      bool has_attack,
                                      bool has_guard_mode,
                                      bool has_hold_mode,
                                      bool has_patrol) {
  if (transform == nullptr) {
    return;
  }

  if (!has_attack && !has_guard_mode && !has_hold_mode && !has_patrol) {
    return;
  }

  float indicator_height = Render::Geom::k_indicator_height_base;
  float const indicator_size = Render::Geom::k_indicator_size;

  if (unit_comp != nullptr) {
    auto troop_type_opt = Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);

    if (troop_type_opt) {
      const auto& nation_reg = Game::Systems::NationRegistry::instance();
      const Game::Systems::Nation* nation =
          nation_reg.get_nation_for_player(unit_comp->owner_id);
      Game::Systems::NationID const nation_id =
          nation != nullptr ? nation->id : nation_reg.default_nation_id();

      const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          nation_id, *troop_type_opt);
      indicator_height = Render::Geom::indicator_height_for_unit(
          profile.visuals.selection_ring_size, profile.visuals.render_scale);
    } else {
      indicator_height = Render::Geom::indicator_height_for_unit(
          Game::Units::TroopConfig::instance().get_selection_ring_size(
              unit_comp->spawn_type),
          1.0F);
    }
  }

  QVector3D const pos(transform->position.x,
                      transform->position.y + indicator_height,
                      transform->position.z);

  if (m_camera != nullptr) {
    QVector4D const clip_pos = m_view_proj * QVector4D(pos, 1.0F);
    if (clip_pos.w() > 0.0F) {
      float const ndc_x = clip_pos.x() / clip_pos.w();
      float const ndc_y = clip_pos.y() / clip_pos.w();
      float const ndc_z = clip_pos.z() / clip_pos.w();

      constexpr float margin = Render::Geom::k_frustum_cull_margin;
      if (ndc_x < -margin || ndc_x > margin || ndc_y < -margin || ndc_y > margin ||
          ndc_z < -1.0F || ndc_z > 1.0F) {
        return;
      }
    }
  }

  QMatrix4x4 indicator_model;
  indicator_model.translate(pos);
  indicator_model.scale(indicator_size, indicator_size, indicator_size);

  if (m_camera != nullptr) {
    QVector3D const cam_pos = m_camera->get_position();
    QVector3D const to_camera = (cam_pos - pos).normalized();

    constexpr float k_pi = std::numbers::pi_v<float>;
    float const yaw = std::atan2(to_camera.x(), to_camera.z());
    indicator_model.rotate(yaw * 180.0F / k_pi, 0, 1, 0);
  }

  int mode_type = Render::Geom::k_mode_type_patrol;
  QVector3D color = Render::Geom::k_patrol_mode_color;

  if (has_hold_mode) {
    mode_type = Render::Geom::k_mode_type_hold;
    color = Render::Geom::k_hold_mode_color;
  }

  if (has_guard_mode) {
    mode_type = Render::Geom::k_mode_type_guard;
    color = Render::Geom::k_guard_mode_color;
  }

  if (has_attack) {
    mode_type = Render::Geom::k_mode_type_attack;
    color = Render::Geom::k_attack_mode_color;
  }

  mode_indicator(indicator_model, mode_type, color, Render::Geom::k_indicator_alpha);
}

void Renderer::render_world(Engine::Core::World* world) {
  if (m_paused.load()) {
    return;
  }
  if (world == nullptr) {
    return;
  }

  std::lock_guard<std::recursive_mutex> const guard(world->get_entity_mutex());

  if (!m_render_registry.is_attached_to(world)) {
    m_cached_world = world;
    m_render_registry.attach(world);
    log_render_first_use_once(
        "render-registry-attach",
        QStringLiteral("attached persistent render registry to world"));
  }

  auto& vis = Game::Map::VisibilityService::instance();
  const bool visibility_enabled = vis.is_initialized();
  const auto visibility_snapshot = visibility_enabled ? vis.snapshot_ptr() : nullptr;
  const Game::Map::VisibilityService::Snapshot empty_visibility_snapshot;
  const auto& resolved_visibility_snapshot =
      visibility_snapshot != nullptr ? *visibility_snapshot : empty_visibility_snapshot;

  const auto& unit_ids = m_render_registry.unit_ids();
  const auto& building_ids = m_render_registry.building_ids();
  const auto& other_ids = m_render_registry.other_ids();

  const auto& gfx_settings = Render::GraphicsSettings::instance();
  const auto& batch_config = gfx_settings.batching_config();

  float camera_height = 0.0F;
  if (m_camera != nullptr) {
    camera_height = m_camera->get_position().y();
  }

  ++m_frame_counter;

  int visible_unit_count = 0;
  static thread_local std::vector<UnitRenderEntry> unit_entries;
  static thread_local std::vector<RenderEntry> building_entries;
  static thread_local std::vector<RenderEntry> other_entries;
  unit_entries.clear();
  building_entries.clear();
  other_entries.clear();
  unit_entries.reserve(unit_ids.size());
  building_entries.reserve(building_ids.size());
  other_entries.reserve(other_ids.size());

  for (std::uint32_t const entity_id : unit_ids) {

    Engine::Core::Entity* entity = world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }
    bool const has_death_motion =
        entity->get_component<Engine::Core::DeathAnimationComponent>() != nullptr;
    if (entity->has_component<Engine::Core::PendingRemovalComponent>() &&
        !has_death_motion) {
      continue;
    }

    auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp != nullptr) && unit_comp->health <= 0 && !has_death_motion) {
      continue;
    }

    if (unit_comp != nullptr) {

      auto& cached =
          m_unit_render_cache.get_or_create(entity_id, entity, m_frame_counter);

      if (cached.renderable == nullptr || !cached.renderable->visible) {
        continue;
      }
      if (cached.transform == nullptr) {
        continue;
      }

      UnitRenderEntry entry;
      entry.entity = entity;
      entry.renderable = cached.renderable;
      entry.transform = cached.transform;
      entry.unit = cached.unit;
      entry.entity_id = entity_id;

      bool const is_selected = (m_selected_ids.find(entity_id) != m_selected_ids.end());
      bool const is_hovered = (entity_id == m_hovered_entity_id);
      entry.selected = is_selected;
      entry.hovered = is_hovered;
      if (m_entity_registry != nullptr && !cached.has_renderer_handle &&
          !cached.renderer_key.empty()) {
        const auto renderer_handle = m_entity_registry->get_handle(cached.renderer_key);
        if (renderer_handle != Render::GL::k_invalid_renderer_handle) {
          cached.renderer_handle = renderer_handle;
          cached.has_renderer_handle = true;
        }
      }
      entry.renderer_key = cached.renderer_key;
      entry.renderer_handle =
          cached.has_renderer_handle
              ? static_cast<Render::GL::RendererHandle>(cached.renderer_handle)
              : Render::GL::k_invalid_renderer_handle;
      entry.movement = cached.movement;
      entry.motion = entity->get_component<Engine::Core::MotionPresentationComponent>();

      UnitRenderCache::update_model_matrix(cached);
      entry.model_matrix = cached.model_matrix;

      if (m_camera != nullptr) {
        QVector3D const unit_pos(cached.transform->position.x,
                                 cached.transform->position.y,
                                 cached.transform->position.z);
        if (m_camera->is_in_frustum(unit_pos, 4.0F)) {
          ++visible_unit_count;
        }

        float const cull_radius = get_unit_cull_radius(unit_comp->spawn_type);
        entry.in_frustum = m_camera->is_in_frustum(unit_pos, cull_radius);

        QVector3D const cam_pos = m_camera->get_position();
        float const dx = unit_pos.x() - cam_pos.x();
        float const dz = unit_pos.z() - cam_pos.z();
        entry.distance_sq = dx * dx + dz * dz;
      }

      if (unit_comp->owner_id != m_local_owner_id && visibility_enabled &&
          non_local_unit_visibility_filter_enabled()) {
        entry.fog_visible =
            Game::Map::should_render_non_local_unit(resolved_visibility_snapshot,
                                                    cached.transform->position.x,
                                                    cached.transform->position.z);
      }

      auto* attack_comp = entity->get_component<Engine::Core::AttackComponent>();
      auto* attack_target =
          entity->get_component<Engine::Core::AttackTargetComponent>();
      auto* combat_state = entity->get_component<Engine::Core::CombatStateComponent>();
      auto* hit_feedback = entity->get_component<Engine::Core::HitFeedbackComponent>();
      entry.combat_active = is_unit_combat_active(
          entity, attack_comp, attack_target, combat_state, hit_feedback);
      entry.has_attack =
          (attack_comp != nullptr) && attack_comp->in_melee_lock &&
          Game::Systems::CombatRules::participates_in_rts_melee_lock(entity);

      auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
      entry.has_guard_mode = (guard_mode != nullptr) && guard_mode->active;

      auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
      entry.has_hold_mode = (hold_mode != nullptr) && hold_mode->active;

      auto* patrol_comp = entity->get_component<Engine::Core::PatrolComponent>();
      entry.has_patrol = (patrol_comp != nullptr) && patrol_comp->patrolling;

      unit_entries.push_back(std::move(entry));
    }
  }

  auto collect_non_unit_entry = [&](std::uint32_t entity_id,
                                    std::vector<RenderEntry>& dest) {
    Engine::Core::Entity* entity = world->get_entity(entity_id);
    if (entity == nullptr) {
      return;
    }
    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      return;
    }

    auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if ((renderable == nullptr) || !renderable->visible) {
      return;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      return;
    }

    RenderEntry entry;
    entry.entity = entity;
    entry.renderable = renderable;
    entry.transform = transform;
    entry.unit = nullptr;
    entry.entity_id = entity_id;
    entry.selected = (m_selected_ids.find(entity_id) != m_selected_ids.end());
    entry.hovered = (entity_id == m_hovered_entity_id);
    if (!renderable->renderer_id.empty()) {
      entry.renderer_key =
          std::string(canonicalize_building_renderer_key(renderable->renderer_id));
      if (m_entity_registry != nullptr) {
        entry.renderer_handle = m_entity_registry->get_handle(entry.renderer_key);
      }
    }
    dest.push_back(std::move(entry));
  };

  for (std::uint32_t const entity_id : building_ids) {
    collect_non_unit_entry(entity_id, building_entries);
  }

  for (std::uint32_t const entity_id : other_ids) {
    collect_non_unit_entry(entity_id, other_entries);
  }

  m_unit_render_cache.prune(m_frame_counter);
  m_model_matrix_cache.prune(m_frame_counter);
  auto& battle_optimizer = Render::BattleRenderOptimizer::instance();
  battle_optimizer.set_visible_unit_count(visible_unit_count);
  uint32_t const optimizer_frame = battle_optimizer.frame_counter();

  float batching_ratio =
      gfx_settings.calculate_batching_ratio(visible_unit_count, camera_height);

  float const batching_boost = battle_optimizer.get_batching_boost();
  batching_ratio = std::min(1.0F, batching_ratio * batching_boost);

  static thread_local PrimitiveBatcher batcher;
  batcher.clear();
  if (batching_ratio > 0.0F) {
    batcher.reserve(2000, 4000, 500);
  }

  float const full_shader_max_distance_sq =
      Render::Pipeline::compute_full_detail_max_distance_sq(
          batching_ratio, batch_config.force_batching);

  BatchingSubmitter batch_submitter(this, &batcher);

  ResourceManager* res = resources();

  auto resolve_fallback_mesh =
      [&](Engine::Core::RenderableComponent* renderable) -> Mesh* {
    Mesh* mesh_to_draw = nullptr;
    switch (renderable->mesh) {
    case Engine::Core::RenderableComponent::MeshKind::Quad:
      mesh_to_draw = (res != nullptr) ? res->quad() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Plane:
      mesh_to_draw = (res != nullptr) ? res->ground() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Cube:
      mesh_to_draw = (res != nullptr) ? res->unit() : nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Capsule:
      mesh_to_draw = nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::Ring:
      mesh_to_draw = nullptr;
      break;
    case Engine::Core::RenderableComponent::MeshKind::None:
    default:
      break;
    }
    if ((mesh_to_draw == nullptr) && (res != nullptr)) {
      mesh_to_draw = res->unit();
    }
    if ((mesh_to_draw == nullptr) && (res != nullptr)) {
      mesh_to_draw = res->quad();
    }
    return mesh_to_draw;
  };

  auto draw_contact_shadow = [&](Engine::Core::TransformComponent* transform,
                                 Engine::Core::UnitComponent* unit_comp,
                                 float distance_sq) {
    if (res == nullptr) {
      return;
    }
    Mesh* contact_quad = res->quad();
    Texture* white = res->white();
    if ((contact_quad == nullptr) || (white == nullptr)) {
      return;
    }
    QMatrix4x4 contact_base;
    contact_base.translate(
        transform->position.x, transform->position.y + 0.03F, transform->position.z);
    constexpr float k_contact_shadow_rotation = -90.0F;
    contact_base.rotate(k_contact_shadow_rotation, 1.0F, 0.0F, 0.0F);
    float const footprint = std::max({transform->scale.x, transform->scale.z, 0.6F});

    float size_ratio = 1.0F;
    if (unit_comp != nullptr) {
      int const mh = std::max(1, unit_comp->max_health);
      size_ratio = std::clamp(unit_comp->health / float(mh), 0.0F, 1.0F);
    }
    float const eased = 0.25F + 0.75F * size_ratio;

    float const base_scale_x = footprint * 0.55F * eased;
    float const base_scale_y = footprint * 0.35F * eased;

    QVector3D const col(0.03F, 0.03F, 0.03F);
    float const center_alpha = 0.32F * eased;
    float const mid_alpha = 0.16F * eased;
    float const outer_alpha = 0.07F * eased;

    int shadow_layers = 3;
    if ((visible_unit_count > 420) || (distance_sq > 1200.0F)) {
      shadow_layers = 1;
    } else if ((visible_unit_count > 260) || (distance_sq > 600.0F)) {
      shadow_layers = 2;
    }

    QMatrix4x4 c0 = contact_base;
    c0.scale(base_scale_x * 0.60F, base_scale_y * 0.60F, 1.0F);
    mesh(contact_quad, c0, col, white, center_alpha);

    if (shadow_layers >= 2) {
      QMatrix4x4 c1 = contact_base;
      c1.scale(base_scale_x * 0.95F, base_scale_y * 0.95F, 1.0F);
      mesh(contact_quad, c1, col, white, mid_alpha);
    }

    if (shadow_layers >= 3) {
      QMatrix4x4 c2 = contact_base;
      c2.scale(base_scale_x * 1.35F, base_scale_y * 1.35F, 1.0F);
      mesh(contact_quad, c2, col, white, outer_alpha);
    }
  };

  for (auto& entry : unit_entries) {
    if (!entry.in_frustum || !entry.fog_visible) {
      continue;
    }

    bool const should_update_temporal =
        battle_optimizer.should_render_unit(entry.entity_id,
                                            entry.motion,
                                            entry.selected,
                                            entry.hovered,
                                            entry.combat_active,
                                            entry.distance_sq);

    const QMatrix4x4& model_matrix = entry.model_matrix;

    bool drawn_by_registry = false;
    bool tier_is_minimal = false;
    if (m_entity_registry) {
      auto const* fn = m_entity_registry->get(entry.renderer_handle);
      if (fn == nullptr && entry.unit != nullptr) {
        const std::string profile_renderer_key =
            Render::resolve_profile_unit_renderer_key(*entry.unit);
        if (!profile_renderer_key.empty() &&
            profile_renderer_key != entry.renderer_key) {
          const auto profile_renderer_handle =
              m_entity_registry->get_handle(profile_renderer_key);
          fn = m_entity_registry->get(profile_renderer_handle);
          if (fn != nullptr) {
            entry.renderer_key = profile_renderer_key;
            entry.renderer_handle = profile_renderer_handle;
          }
        }
      }
      if (fn != nullptr) {
        DrawContext ctx{resources(), entry.entity, world, model_matrix};

        ctx.selected = entry.selected;
        ctx.hovered = entry.hovered;
        bool should_update_animation = true;
        if (should_update_temporal) {
          should_update_animation =
              battle_optimizer.should_update_animation(entry.entity_id,
                                                       entry.distance_sq,
                                                       entry.selected,
                                                       entry.combat_active,
                                                       entry.motion);
        } else {
          should_update_animation = false;
        }

        float const animation_time = resolve_animation_time(entry.entity_id,
                                                            should_update_animation,
                                                            m_accumulated_time,
                                                            optimizer_frame);

        ctx.animation_time = animation_time;
        ctx.distance_sq = entry.distance_sq;
        ctx.renderer_id = entry.renderer_key;
        ctx.renderer_handle = entry.renderer_handle;
        ctx.backend = m_gl_backend;
        ctx.camera = m_camera;
        ctx.animation_throttled = !should_update_animation;

        Render::Pipeline::LodInputs lod_in;
        lod_in.distance_sq = entry.distance_sq;
        lod_in.visible_unit_count = visible_unit_count;
        lod_in.full_detail_max_distance_sq = full_shader_max_distance_sq;
        lod_in.selected = entry.selected;
        lod_in.hovered = entry.hovered;
        lod_in.in_frustum = entry.in_frustum;
        lod_in.fog_visible = entry.fog_visible;
        lod_in.force_batching = batch_config.force_batching;
        lod_in.never_batch = batch_config.never_batch;

        if (m_force_full_creature_lod) {
          ctx.force_humanoid_lod = true;
          ctx.forced_humanoid_lod = HumanoidLOD::Full;
          ctx.force_horse_lod = true;
          ctx.forced_horse_lod = HorseLOD::Full;
        } else if (entry.combat_active) {
          auto const stable_lod =
              stable_combat_creature_lod(entry.unit, entry.distance_sq);
          ctx.force_humanoid_lod = true;
          ctx.forced_humanoid_lod = stable_lod;
          ctx.force_horse_lod = true;
          ctx.forced_horse_lod = stable_lod;
        }

        const bool batching_available =
            !m_force_full_creature_lod && !entry.combat_active && batching_ratio > 0.0F;
        const auto tier = m_force_full_creature_lod
                              ? Render::Pipeline::LodTier::Full
                              : Render::Pipeline::select_lod(lod_in);

        const bool use_batching =
            batching_available && (tier == Render::Pipeline::LodTier::Simplified ||
                                   tier == Render::Pipeline::LodTier::Minimal);
        tier_is_minimal = tier == Render::Pipeline::LodTier::Minimal;
        if (use_batching) {
          (*fn)(ctx, batch_submitter);
        } else {
          (*fn)(ctx, *this);
        }

        drawn_by_registry = true;
      }
    }
    if (drawn_by_registry) {

      if (entry.selected || entry.hovered) {
        enqueue_selection_ring(
            entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
      }
      if (!tier_is_minimal) {
        enqueue_mode_indicator(entry.transform,
                               entry.unit,
                               entry.has_attack,
                               entry.has_guard_mode,
                               entry.has_hold_mode,
                               entry.has_patrol);
      }
      continue;
    }

    Mesh* mesh_to_draw = resolve_fallback_mesh(entry.renderable);
    QVector3D const color = QVector3D(entry.renderable->color[0],
                                      entry.renderable->color[1],
                                      entry.renderable->color[2]);

    draw_contact_shadow(entry.transform, entry.unit, entry.distance_sq);
    if (entry.selected || entry.hovered) {
      enqueue_selection_ring(
          entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
    }
    enqueue_mode_indicator(entry.transform,
                           entry.unit,
                           entry.has_attack,
                           entry.has_guard_mode,
                           entry.has_hold_mode,
                           entry.has_patrol);
    mesh(mesh_to_draw,
         model_matrix,
         color,
         (res != nullptr) ? res->white() : nullptr,
         1.0F);
  }

  auto render_non_unit_entry = [&](const RenderEntry& entry) {
    const QMatrix4x4& model_matrix = m_model_matrix_cache.get_or_create(
        entry.entity_id, entry.transform, m_frame_counter);

    bool drawn_by_registry = false;
    if (m_entity_registry &&
        entry.renderer_handle != Render::GL::k_invalid_renderer_handle) {
      auto const* fn = m_entity_registry->get(entry.renderer_handle);
      if (fn != nullptr) {
        DrawContext ctx{resources(), entry.entity, world, model_matrix};
        ctx.selected = entry.selected;
        ctx.hovered = entry.hovered;
        ctx.animation_time = m_accumulated_time;
        ctx.renderer_id = entry.renderer_key;
        ctx.renderer_handle = entry.renderer_handle;
        ctx.backend = m_gl_backend;
        ctx.camera = m_camera;
        ctx.animation_throttled = false;
        (*fn)(ctx, *this);
        drawn_by_registry = true;
      }
    }
    if (drawn_by_registry) {
      if (entry.selected || entry.hovered) {
        enqueue_selection_ring(
            entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
      }
      return;
    }

    Mesh* mesh_to_draw = resolve_fallback_mesh(entry.renderable);
    QVector3D const color = QVector3D(entry.renderable->color[0],
                                      entry.renderable->color[1],
                                      entry.renderable->color[2]);

    draw_contact_shadow(entry.transform, entry.unit, 0.0F);
    if (entry.selected || entry.hovered) {
      enqueue_selection_ring(
          entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
    }
    mesh(mesh_to_draw,
         model_matrix,
         color,
         (res != nullptr) ? res->white() : nullptr,
         1.0F);
  };

  for (const auto& entry : building_entries) {
    render_non_unit_entry(entry);
  }

  for (const auto& entry : other_entries) {
    render_non_unit_entry(entry);
  }

  {
    Render::Pass::FrameContext pass_ctx;
    pass_ctx.renderer = this;
    pass_ctx.world = world;
    pass_ctx.queue = m_active_queue;
    pass_ctx.frame_counter = m_frame_counter;
    pass_ctx.view_proj = m_view_proj;
    pass_ctx.primitive_batcher = &batcher;
    pass_ctx.visibility = const_cast<Game::Map::VisibilityService*>(&vis);
    pass_ctx.visibility_enabled = visibility_enabled;
    pass_ctx.light_direction = m_light_dir;
    pass_ctx.ambient_strength = m_ambient_strength;

    Render::Pass::FramePassRunner runner;
    runner.add(std::make_unique<Render::Pass::PrimitiveFlushPass>());
    runner.add(std::make_unique<Render::Pass::ConstructionPreviewPass>());
    runner.execute(pass_ctx);
  }
}

void Renderer::prewarm_unit_templates(
    Engine::Core::World* world, TemplatePrewarmProgressCallback progress_callback) {
  cancel_async_template_prewarm();
  Render::Creature::set_runtime_bake_forbidden(false);
  m_forbid_runtime_bake_when_async_prewarm_done = false;
  if (!m_entity_registry) {
    return;
  }

  auto report_progress = [&](TemplatePrewarmProgress::Phase phase,
                             std::size_t completed,
                             std::size_t total) -> bool {
    if (!progress_callback) {
      return true;
    }
    TemplatePrewarmProgress progress;
    progress.phase = phase;
    progress.completed = completed;
    progress.total = total;
    return progress_callback(progress);
  };

  if (!report_progress(TemplatePrewarmProgress::Phase::CollectingProfiles, 0, 0)) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, 0, 0);
    return;
  }

  struct PrewarmProfileKey {
    Render::GL::RendererHandle renderer_handle{Render::GL::k_invalid_renderer_handle};
    Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
    Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
    bool operator==(const PrewarmProfileKey& other) const {
      return renderer_handle == other.renderer_handle &&
             spawn_type == other.spawn_type && nation_id == other.nation_id;
    }
  };

  struct PrewarmProfileKeyHash {
    std::size_t operator()(const PrewarmProfileKey& key) const noexcept {
      std::size_t h = static_cast<std::size_t>(key.renderer_handle);
      h ^= static_cast<std::size_t>(key.spawn_type) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= static_cast<std::size_t>(static_cast<std::uint8_t>(key.nation_id)) +
           0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };

  auto is_prewarmable_spawn = [](Game::Units::SpawnType spawn_type) -> bool {
    using Game::Units::SpawnType;
    switch (spawn_type) {
    case SpawnType::Archer:
    case SpawnType::Knight:
    case SpawnType::Spearman:
    case SpawnType::MountedKnight:
    case SpawnType::HorseArcher:
    case SpawnType::HorseSpearman:
    case SpawnType::Healer:
    case SpawnType::Civilian:
    case SpawnType::Builder:
    case SpawnType::Elephant:
      return true;
    case SpawnType::Catapult:
    case SpawnType::Ballista:
    default:
      return false;
    }
  };

  auto is_prewarmable_troop = [](Game::Units::TroopType type) -> bool {
    using Game::Units::TroopType;
    switch (type) {
    case TroopType::Archer:
    case TroopType::Swordsman:
    case TroopType::Spearman:
    case TroopType::MountedKnight:
    case TroopType::HorseArcher:
    case TroopType::HorseSpearman:
    case TroopType::Healer:
    case TroopType::Civilian:
    case TroopType::Builder:
    case TroopType::Elephant:
      return true;
    case TroopType::Catapult:
    case TroopType::Ballista:
    default:
      return false;
    }
  };

  auto choose_template_budget_by_quality = []() -> std::size_t {
    using Render::GraphicsQuality;
    switch (Render::GraphicsSettings::instance().quality()) {
    case GraphicsQuality::Low:
      return 256;
    case GraphicsQuality::Medium:
      return 512;
    case GraphicsQuality::High:
      return 1'024;
    case GraphicsQuality::Ultra:
    default:
      return 2'048;
    }
  };

  std::vector<int> owner_ids;
  owner_ids.reserve(8);
  std::unordered_set<int> owner_seen;
  std::unordered_set<Game::Systems::NationID> active_nation_ids;
  std::vector<PrewarmProfile> profiles;
  profiles.reserve(64);
  std::unordered_set<PrewarmProfileKey, PrewarmProfileKeyHash> profile_seen;

  auto add_owner = [&](int owner_id) {
    if (owner_seen.insert(owner_id).second) {
      owner_ids.push_back(owner_id);
    }
  };

  auto add_profile = [&](const std::string& renderer_id,
                         Game::Units::SpawnType spawn_type,
                         Game::Systems::NationID nation_id,
                         int max_health,
                         bool is_building = false) {
    std::string canonical_renderer_id(renderer_id);
    if (renderer_id.empty() && is_building) {
      canonical_renderer_id =
          building_renderer_key(nation_id, Game::Units::spawn_typeToString(spawn_type));
    } else if (is_building) {
      canonical_renderer_id = resolve_building_renderer_key(
          renderer_id, Game::Units::spawn_typeToString(spawn_type), nation_id);
    } else {
      canonical_renderer_id =
          std::string(canonicalize_building_renderer_key(renderer_id));
    }
    if (!is_prewarmable_spawn(spawn_type) || canonical_renderer_id.empty()) {
      return;
    }
    const auto renderer_handle = m_entity_registry->get_handle(canonical_renderer_id);
    if (renderer_handle == Render::GL::k_invalid_renderer_handle) {
      return;
    }
    PrewarmProfileKey const key{renderer_handle, spawn_type, nation_id};
    if (!profile_seen.insert(key).second) {
      return;
    }
    auto const* fn = m_entity_registry->get(renderer_handle);
    if (fn == nullptr) {
      return;
    }

    PrewarmProfile p;
    p.renderer_id = canonical_renderer_id;
    p.spawn_type = spawn_type;
    p.nation_id = nation_id;
    p.max_health = std::max(1, max_health);
    p.is_elephant = (spawn_type == Game::Units::SpawnType::Elephant);
    p.is_mounted = (spawn_type == Game::Units::SpawnType::MountedKnight ||
                    spawn_type == Game::Units::SpawnType::HorseArcher ||
                    spawn_type == Game::Units::SpawnType::HorseSpearman);
    p.fn = *fn;
    profiles.push_back(std::move(p));
  };

  if (world != nullptr) {
    auto world_units = world->get_entities_with<Engine::Core::UnitComponent>();
    for (auto* entity : world_units) {
      if (entity == nullptr ||
          entity->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
      if (unit == nullptr || renderable == nullptr || unit->health <= 0) {
        continue;
      }

      if (!is_prewarmable_spawn(unit->spawn_type)) {
        continue;
      }

      add_owner(unit->owner_id);
      active_nation_ids.insert(unit->nation_id);
      add_profile(renderable->renderer_id,
                  unit->spawn_type,
                  unit->nation_id,
                  unit->max_health,
                  entity->get_component<Engine::Core::BuildingComponent>() != nullptr);
    }
  }

  if (owner_ids.empty()) {
    const auto& owners = Game::Systems::OwnerRegistry::instance().get_all_owners();
    for (const auto& owner : owners) {
      add_owner(owner.owner_id);
    }
  }
  if (owner_ids.empty()) {
    add_owner(0);
  }

  {
    const auto& troops = Game::Units::TroopCatalog::instance().get_all_classes();
    const auto& nations = Game::Systems::NationRegistry::instance().get_all_nations();
    const bool restrict_to_active_nations = !active_nation_ids.empty();

    for (const auto& nation : nations) {
      if (restrict_to_active_nations &&
          (active_nation_ids.find(nation.id) == active_nation_ids.end())) {
        continue;
      }
      for (const auto& entry : troops) {
        auto type = entry.first;
        if (!is_prewarmable_troop(type)) {
          continue;
        }

        auto profile =
            Game::Systems::TroopProfileService::instance().get_profile(nation.id, type);
        if (profile.visuals.renderer_id.empty()) {
          continue;
        }

        add_profile(profile.visuals.renderer_id,
                    Game::Units::spawn_typeFromTroopType(type),
                    nation.id,
                    profile.combat.max_health);
      }
    }
  }

  if (profiles.empty()) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  auto profile_priority = [](const PrewarmProfile& profile) -> int {
    if (profile.is_elephant) {
      return 0;
    }
    if (profile.is_mounted) {
      return 1;
    }
    return 2;
  };
  std::stable_sort(profiles.begin(),
                   profiles.end(),
                   [&](const PrewarmProfile& lhs, const PrewarmProfile& rhs) {
                     const int lp = profile_priority(lhs);
                     const int rp = profile_priority(rhs);
                     if (lp != rp) {
                       return lp < rp;
                     }
                     return lhs.renderer_id < rhs.renderer_id;
                   });

  auto preload_profile_archetypes = [&]() {
    if (owner_ids.empty()) {
      return;
    }

    AnimKey idle_key{};
    idle_key.state = Render::Creature::PoseIntent::Idle;
    idle_key.combat_phase = CombatAnimPhase::Idle;
    const int owner_id = owner_ids.front();

    for (std::size_t profile_idx = 0; profile_idx < profiles.size(); ++profile_idx) {
      const PrewarmProfile& profile = profiles[profile_idx];
      for (std::uint8_t variant = 0; variant < k_template_variant_count; ++variant) {
        execute_template_prewarm_item(*this,
                                      profile_idx,
                                      profile.spawn_type,
                                      profile.nation_id,
                                      profile.max_health,
                                      profile.renderer_id,
                                      profile.is_mounted,
                                      profile.is_elephant,
                                      profile.fn,
                                      owner_id,
                                      HumanoidLOD::Full,
                                      variant,
                                      false,
                                      idle_key);
      }
    }
  };

  preload_profile_archetypes();

  const std::size_t domain_count = profiles.size() * owner_ids.size() * 3U;
  if (domain_count == 0) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  const std::size_t target_template_count = choose_template_budget_by_quality();
  auto const anim_catalog = build_template_prewarm_anim_catalog(
      Render::Creature::ArchetypeRegistry::instance());
  auto const anim_selection = select_template_prewarm_anim_budget(
      domain_count, target_template_count, anim_catalog);

  if (anim_selection.selected_core_keys.empty() ||
      anim_selection.variant_values.empty()) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  clear_humanoid_caches();

  std::vector<PrewarmWorkItem> core_work_items =
      build_template_prewarm_work_items(profiles,
                                        owner_ids,
                                        anim_selection.variant_values,
                                        anim_selection.selected_core_keys);
  std::vector<PrewarmWorkItem> const extended_work_items =
      build_template_prewarm_work_items(profiles,
                                        owner_ids,
                                        anim_selection.variant_values,
                                        anim_selection.selected_extra_keys);

  const std::size_t total_work_count =
      core_work_items.size() + extended_work_items.size();
  if (core_work_items.empty()) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, total_work_count);
    return;
  }

  if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                       0,
                       core_work_items.size())) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, 0, total_work_count);
    return;
  }

  std::vector<std::mutex> profile_mutexes(profiles.size());

  const std::size_t worker_count = 1U;

  std::atomic<std::size_t> next_index{0};
  std::atomic<std::size_t> completed_count{0};
  std::atomic<bool> cancel_requested{false};

  auto worker = [&]() {
    while (true) {
      if (cancel_requested.load(std::memory_order_relaxed)) {
        break;
      }
      const std::size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
      if (idx >= core_work_items.size()) {
        break;
      }

      const PrewarmWorkItem& item = core_work_items[idx];
      const PrewarmProfile& profile = profiles[item.profile_index];

      std::lock_guard<std::mutex> const profile_lock(
          profile_mutexes[item.profile_index]);
      execute_template_prewarm_item(*this,
                                    item.profile_index,
                                    profile.spawn_type,
                                    profile.nation_id,
                                    profile.max_health,
                                    profile.renderer_id,
                                    profile.is_mounted,
                                    profile.is_elephant,
                                    profile.fn,
                                    item.owner_id,
                                    item.lod,
                                    item.variant,
                                    true,
                                    item.anim_key);
      completed_count.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> workers;
  workers.reserve(worker_count);
  if (worker_count == 1U) {
    worker();
  } else {
    for (std::size_t i = 0; i < worker_count; ++i) {
      workers.emplace_back(worker);
    }
  }

  constexpr std::size_t k_progress_report_step = 2048;
  std::size_t last_reported = 0;
  while (!cancel_requested.load(std::memory_order_relaxed)) {
    const std::size_t done = std::min(completed_count.load(std::memory_order_relaxed),
                                      core_work_items.size());
    if (done >= core_work_items.size()) {
      break;
    }

    if ((done - last_reported) >= k_progress_report_step) {
      last_reported = done;
      if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                           done,
                           core_work_items.size())) {
        cancel_requested.store(true, std::memory_order_relaxed);
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  for (auto& t : workers) {
    t.join();
  }

  const std::size_t core_done =
      std::min(completed_count.load(std::memory_order_relaxed), core_work_items.size());
  if (cancel_requested.load(std::memory_order_relaxed) ||
      core_done < core_work_items.size()) {
    report_progress(
        TemplatePrewarmProgress::Phase::Cancelled, core_done, total_work_count);
    return;
  }
  if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                       core_done,
                       core_work_items.size())) {
    report_progress(
        TemplatePrewarmProgress::Phase::Cancelled, core_done, total_work_count);
    return;
  }

  if (!extended_work_items.empty()) {
    if (!report_progress(TemplatePrewarmProgress::Phase::QueueingExtendedTemplates,
                         extended_work_items.size(),
                         extended_work_items.size())) {
      report_progress(
          TemplatePrewarmProgress::Phase::Cancelled, core_done, total_work_count);
      return;
    }

    auto async_state = std::make_shared<AsyncTemplatePrewarmState>();
    async_state->profiles = profiles;
    async_state->work_items = extended_work_items;

    {
      std::lock_guard<std::mutex> const lock(m_async_prewarm_mutex);
      m_async_prewarm_state = std::move(async_state);
      m_forbid_runtime_bake_when_async_prewarm_done = true;
    }
  } else {
    Render::Creature::set_runtime_bake_forbidden(true);
  }

  report_progress(
      TemplatePrewarmProgress::Phase::Completed, core_done, total_work_count);
}

void Renderer::render_construction_previews(Engine::Core::World* world,
                                            const Game::Map::VisibilityService* vis,
                                            bool visibility_enabled) {
  if (world == nullptr || m_entity_registry == nullptr) {
    return;
  }

  const auto visibility_snapshot =
      (visibility_enabled && vis != nullptr) ? vis->snapshot_ptr() : nullptr;
  const Game::Map::VisibilityService::Snapshot empty_visibility_snapshot;
  const auto& resolved_visibility_snapshot =
      visibility_snapshot != nullptr ? *visibility_snapshot : empty_visibility_snapshot;

  auto builders = world->get_entities_with<Engine::Core::BuilderProductionComponent>();

  for (auto* builder : builders) {
    if (builder->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* builder_prod =
        builder->get_component<Engine::Core::BuilderProductionComponent>();
    auto* transform = builder->get_component<Engine::Core::TransformComponent>();
    auto* unit_comp = builder->get_component<Engine::Core::UnitComponent>();

    if (builder_prod == nullptr || transform == nullptr) {
      continue;
    }

    bool show_preview = false;
    float preview_x = transform->position.x;
    float preview_z = transform->position.z;

    if (builder_prod->is_placement_preview && builder_prod->has_construction_site) {
      show_preview = true;
      preview_x = builder_prod->construction_site_x;
      preview_z = builder_prod->construction_site_z;
    } else if (builder_prod->is_placement_preview && builder_prod->in_progress) {
      show_preview = true;
    }

    if (!show_preview) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->owner_id != m_local_owner_id) {
      if (visibility_enabled &&
          !Game::Map::should_render_non_local_preview(
              resolved_visibility_snapshot, preview_x, preview_z)) {
        continue;
      }
    }

    if (m_camera != nullptr) {
      QVector3D const pos(preview_x, transform->position.y, preview_z);
      if (!m_camera->is_in_frustum(pos, 5.0F)) {
        continue;
      }
    }

    Game::Systems::NationID preview_nation = Game::Systems::NationID::RomanRepublic;
    if (unit_comp != nullptr) {
      preview_nation = unit_comp->nation_id;
    }

    std::string const renderer_key =
        building_renderer_key(preview_nation, builder_prod->product_type);

    const auto renderer_handle = m_entity_registry->get_handle(renderer_key);
    auto const* fn = m_entity_registry->get(renderer_handle);
    if (fn == nullptr) {
      continue;
    }

    auto& terrain_service = Game::Map::TerrainService::instance();
    QVector3D const preview_pos = terrain_service.resolve_surface_world_position(
        preview_x, preview_z, 0.0F, 0.0F);

    QMatrix4x4 model_matrix;
    model_matrix.translate(preview_pos);

    DrawContext ctx{resources(), builder, world, model_matrix};
    ctx.selected = false;
    ctx.hovered = false;
    ctx.animation_time = m_accumulated_time;
    ctx.renderer_id = renderer_key;
    ctx.renderer_handle = renderer_handle;
    ctx.backend = m_gl_backend;
    ctx.camera = m_camera;

    float const prev_alpha = m_alpha_override;
    m_alpha_override = 0.60F;

    (*fn)(ctx, *this);

    m_alpha_override = prev_alpha;
  }
}

} // namespace Render::GL
