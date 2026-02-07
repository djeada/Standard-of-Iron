#include "scene_renderer.h"
#include "../game/map/terrain_service.h"
#include "../game/map/visibility_service.h"
#include "../game/systems/nation_registry.h"
#include "../game/systems/owner_registry.h"
#include "../game/systems/troop_profile_service.h"
#include "../game/units/spawn_type.h"
#include "../game/units/troop_catalog.h"
#include "../game/units/troop_config.h"
#include "../game/visuals/team_colors.h"
#include "battle_render_optimizer.h"
#include "draw_queue.h"
#include "elephant/rig.h"
#include "entity/registry.h"
#include "equipment/equipment_registry.h"
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
#include "ground/firecamp_gpu.h"
#include "ground/grass_gpu.h"
#include "ground/pine_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/stone_gpu.h"
#include "ground/terrain_gpu.h"
#include "horse/rig.h"
#include "humanoid/rig.h"
#include "pose_palette_cache.h"
#include "primitive_batch.h"
#include "submitter.h"
#include "template_cache.h"
#include "visibility_budget.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>
#include <qvectornd.h>
#include <string>

namespace Render::GL {

namespace {
const QVector3D k_axis_x(1.0F, 0.0F, 0.0F);
const QVector3D k_axis_y(0.0F, 1.0F, 0.0F);
const QVector3D k_axis_z(0.0F, 0.0F, 1.0F);
constexpr uint32_t k_animation_cache_cleanup_mask = 0xFF;
constexpr uint32_t k_animation_cache_max_age = 240;

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

auto is_unit_moving(const Engine::Core::MovementComponent *move_comp) -> bool {
  if (move_comp == nullptr) {
    return false;
  }
  return move_comp->has_target || (std::abs(move_comp->vx) > 0.01F) ||
         (std::abs(move_comp->vz) > 0.01F);
}

struct UnitRenderEntry {
  Engine::Core::Entity *entity{nullptr};
  Engine::Core::RenderableComponent *renderable{nullptr};
  Engine::Core::TransformComponent *transform{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
  Engine::Core::MovementComponent *movement{nullptr};
  std::string renderer_key;
  QMatrix4x4 model_matrix;
  uint32_t entity_id{0};
  bool selected{false};
  bool hovered{false};
  bool moving{false};
  bool in_frustum{true};
  bool fog_visible{true};
  bool has_attack{false};
  bool has_guard_mode{false};
  bool has_hold_mode{false};
  bool has_patrol{false};
  float distance_sq{0.0F};
};

struct RenderEntry {
  Engine::Core::Entity *entity{nullptr};
  Engine::Core::RenderableComponent *renderable{nullptr};
  Engine::Core::TransformComponent *transform{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
  std::string renderer_key;
  uint32_t entity_id{0};
  bool selected{false};
  bool hovered{false};
};
} // namespace

Renderer::Renderer() { m_active_queue = &m_queues[m_fill_queue_index]; }

Renderer::~Renderer() { shutdown(); }

auto Renderer::initialize() -> bool {
  if (!m_backend) {
    m_backend = std::make_shared<Backend>();
  }
  m_backend->initialize();
  m_entity_registry = std::make_unique<EntityRendererRegistry>();
  register_built_in_entity_renderers(*m_entity_registry);
  register_built_in_equipment();
  return true;
}

void Renderer::shutdown() {
  cancel_async_template_prewarm();
  m_backend.reset();
}

void Renderer::begin_frame() {

  advance_pose_cache_frame();
  advance_horse_profile_cache_frame();
  advance_elephant_profile_cache_frame();

  reset_humanoid_render_stats();
  reset_horse_render_stats();
  reset_elephant_render_stats();

  Render::VisibilityBudgetTracker::instance().reset_frame();
  auto &battle_optimizer = Render::BattleRenderOptimizer::instance();
  battle_optimizer.begin_frame();
  prune_animation_time_cache(battle_optimizer.frame_counter());

  m_active_queue = &m_queues[m_fill_queue_index];
  m_active_queue->clear();

  if (m_camera != nullptr) {
    m_view_proj =
        m_camera->get_projection_matrix() * m_camera->get_view_matrix();
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
    std::swap(m_fill_queue_index, m_render_queue_index);
    DrawQueue &render_queue = m_queues[m_render_queue_index];
    render_queue.sort_for_batching();
    m_backend->set_animation_time(m_accumulated_time);
    m_backend->execute(render_queue, *m_camera);
  }
}

void Renderer::set_camera(Camera *camera) { m_camera = camera; }

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
    m_camera->set_perspective(m_camera->get_fov(), aspect, m_camera->get_near(),
                              m_camera->get_far());
  }
}

auto Renderer::resolve_animation_time(uint32_t entity_id, bool update,
                                      float current_time,
                                      uint32_t frame) -> float {
  if (entity_id == 0U) {
    return current_time;
  }

  auto &entry = m_animation_time_cache[entity_id];
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

  for (auto it = m_animation_time_cache.begin();
       it != m_animation_time_cache.end();) {
    if (frame - it->second.last_frame > k_animation_cache_max_age) {
      it = m_animation_time_cache.erase(it);
    } else {
      ++it;
    }
  }
}

void Renderer::mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
                    Texture *texture, float alpha, int material_id) {
  if (mesh == nullptr) {
    return;
  }

  float const effective_alpha = alpha * m_alpha_override;

  static Mesh *const unit_cylinder_mesh = get_unit_cylinder();
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

void Renderer::cylinder(const QVector3D &start, const QVector3D &end,
                        float radius, const QVector3D &color, float alpha) {

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

void Renderer::fog_batch(const FogInstanceData *instances, std::size_t count) {
  if ((instances == nullptr) || count == 0 || (m_active_queue == nullptr)) {
    return;
  }
  FogBatchCmd cmd;
  cmd.instances = instances;
  cmd.count = count;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::grass_batch(Buffer *instance_buffer, std::size_t instance_count,
                           const GrassBatchParams &params) {
  if ((instance_buffer == nullptr) || instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  GrassBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulated_time;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::stone_batch(Buffer *instance_buffer, std::size_t instance_count,
                           const StoneBatchParams &params) {
  if ((instance_buffer == nullptr) || instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  StoneBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::plant_batch(Buffer *instance_buffer, std::size_t instance_count,
                           const PlantBatchParams &params) {
  if ((instance_buffer == nullptr) || instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  PlantBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulated_time;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::pine_batch(Buffer *instance_buffer, std::size_t instance_count,
                          const PineBatchParams &params) {
  if ((instance_buffer == nullptr) || instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  PineBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulated_time;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::olive_batch(Buffer *instance_buffer, std::size_t instance_count,
                           const OliveBatchParams &params) {
  if ((instance_buffer == nullptr) || instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  OliveBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulated_time;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::firecamp_batch(Buffer *instance_buffer,
                              std::size_t instance_count,
                              const FireCampBatchParams &params) {
  if ((instance_buffer == nullptr) || instance_count == 0 ||
      (m_active_queue == nullptr)) {
    return;
  }
  FireCampBatchCmd cmd;
  cmd.instance_buffer = instance_buffer;
  cmd.instance_count = instance_count;
  cmd.params = params;
  cmd.params.time = m_accumulated_time;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::rain_batch(Buffer *instance_buffer, std::size_t instance_count,
                          const RainBatchParams &params) {
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

void Renderer::terrain_chunk(Mesh *mesh, const QMatrix4x4 &model,
                             const TerrainChunkParams &params,
                             std::uint16_t sort_key, bool depth_write,
                             float depth_bias) {
  if ((mesh == nullptr) || (m_active_queue == nullptr)) {
    return;
  }
  TerrainChunkCmd cmd;
  cmd.mesh = mesh;
  cmd.model = model;
  cmd.params = params;
  cmd.sort_key = sort_key;
  cmd.depth_write = depth_write;
  cmd.depth_bias = depth_bias;
  m_active_queue->submit(std::move(cmd));
}

void Renderer::cancel_async_template_prewarm() {
  std::shared_ptr<AsyncTemplatePrewarmState> state;
  {
    std::lock_guard<std::mutex> lock(m_async_prewarm_mutex);
    state = std::move(m_async_prewarm_state);
  }
  if (state) {
    state->cancel_requested.store(true, std::memory_order_relaxed);
  }
}

void Renderer::run_template_prewarm_item(const AsyncPrewarmProfile &profile,
                                         const AsyncPrewarmWorkItem &item) {
  if (!m_entity_registry || !m_backend) {
    return;
  }

  auto fn = m_entity_registry->get(profile.renderer_id);
  if (!fn) {
    return;
  }

  Engine::Core::Entity entity(1);
  auto *unit = entity.add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = static_cast<Game::Units::SpawnType>(profile.spawn_type);
  unit->owner_id = item.owner_id;
  unit->nation_id = static_cast<Game::Systems::NationID>(profile.nation_id);
  unit->max_health = std::max(1, profile.max_health);
  unit->health = unit->max_health;

  auto *transform = entity.add_component<Engine::Core::TransformComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation = {0.0F, 0.0F, 0.0F};
  transform->scale = {1.0F, 1.0F, 1.0F};

  auto *renderable = entity.add_component<Engine::Core::RenderableComponent>(
      "", "");
  renderable->renderer_id = profile.renderer_id;
  renderable->visible = true;
  const QVector3D team_color = Game::Visuals::team_colorForOwner(item.owner_id);
  renderable->color[0] = team_color.x();
  renderable->color[1] = team_color.y();
  renderable->color[2] = team_color.z();

  DrawContext ctx{resources(), &entity, nullptr, QMatrix4x4()};
  ctx.renderer_id = profile.renderer_id;
  ctx.backend = m_backend.get();
  ctx.camera = nullptr;
  ctx.allow_template_cache = true;
  ctx.template_prewarm = true;
  ctx.has_variant_override = true;
  ctx.variant_override = item.variant;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = static_cast<HumanoidLOD>(item.lod);
  ctx.force_horse_lod = profile.is_mounted || profile.is_elephant;
  if (ctx.force_horse_lod) {
    ctx.forced_horse_lod = static_cast<HorseLOD>(item.lod);
  }

  AnimKey anim_key{};
  anim_key.state = static_cast<AnimState>(item.anim_state);
  anim_key.combat_phase = static_cast<CombatAnimPhase>(item.combat_phase);
  anim_key.frame = item.frame;
  anim_key.attack_variant = item.attack_variant;
  AnimationInputs anim = make_animation_inputs(anim_key);
  ctx.animation_override = &anim;
  const bool attack_state = (anim_key.state == AnimState::AttackMelee) ||
                            (anim_key.state == AnimState::AttackRanged);
  ctx.has_attack_variant_override = attack_state;
  ctx.attack_variant_override = anim_key.attack_variant;

  TemplateRecorder recorder;
  recorder.reset();
  fn(ctx, recorder);
}

void Renderer::process_async_template_prewarm() {
  std::shared_ptr<AsyncTemplatePrewarmState> state;
  {
    std::lock_guard<std::mutex> lock(m_async_prewarm_mutex);
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

  std::size_t processed = 0;
  const auto start_time = std::chrono::steady_clock::now();
  while (!state->cancel_requested.load(std::memory_order_relaxed) &&
         processed < max_items) {
    const std::size_t idx =
        state->next_index.fetch_add(1, std::memory_order_relaxed);
    if (idx >= state->work_items.size()) {
      break;
    }

    const auto &item = state->work_items[idx];
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
      (state->next_index.load(std::memory_order_relaxed) >=
       state->work_items.size())) {
    std::lock_guard<std::mutex> lock(m_async_prewarm_mutex);
    if (m_async_prewarm_state == state) {
      m_async_prewarm_state.reset();
    }
  }
}

void Renderer::selection_ring(const QMatrix4x4 &model, float alpha_inner,
                              float alpha_outer, const QVector3D &color) {
  SelectionRingCmd cmd;
  cmd.model = model;
  cmd.alpha_inner = alpha_inner;
  cmd.alpha_outer = alpha_outer;
  cmd.color = color;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::grid(const QMatrix4x4 &model, const QVector3D &color,
                    float cell_size, float thickness, float extent) {
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

void Renderer::selection_smoke(const QMatrix4x4 &model, const QVector3D &color,
                               float base_alpha) {
  SelectionSmokeCmd cmd;
  cmd.model = model;
  cmd.color = color;
  cmd.base_alpha = base_alpha;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::healing_beam(const QVector3D &start, const QVector3D &end,
                            const QVector3D &color, float progress,
                            float beam_width, float intensity, float time) {
  HealingBeamCmd cmd;
  cmd.start_pos = start;
  cmd.end_pos = end;
  cmd.color = color;
  cmd.progress = progress;
  cmd.beam_width = beam_width;
  cmd.intensity = intensity;
  cmd.time = time;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::healer_aura(const QVector3D &position, const QVector3D &color,
                           float radius, float intensity, float time) {
  HealerAuraCmd cmd;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::combat_dust(const QVector3D &position, const QVector3D &color,
                           float radius, float intensity, float time) {
  CombatDustCmd cmd;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::building_flame(const QVector3D &position, const QVector3D &color,
                              float radius, float intensity, float time) {
  BuildingFlameCmd cmd;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::stone_impact(const QVector3D &position, const QVector3D &color,
                            float radius, float intensity, float time) {
  StoneImpactCmd cmd;
  cmd.position = position;
  cmd.color = color;
  cmd.radius = radius;
  cmd.intensity = intensity;
  cmd.time = time;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::mode_indicator(const QMatrix4x4 &model, int mode_type,
                              const QVector3D &color, float alpha) {
  ModeIndicatorCmd cmd;
  cmd.model = model;
  cmd.mode_type = mode_type;
  cmd.color = color;
  cmd.alpha = alpha;
  if (m_active_queue != nullptr) {
    m_active_queue->submit(std::move(cmd));
  }
}

void Renderer::enqueue_selection_ring(
    Engine::Core::Entity *, Engine::Core::TransformComponent *transform,
    Engine::Core::UnitComponent *unit_comp, bool selected, bool hovered) {
  if ((!selected && !hovered) || (transform == nullptr)) {
    return;
  }

  float ring_size = 0.5F;
  float ring_offset = 0.05F;
  float ground_offset = 0.0F;
  float scale_y = 1.0F;

  if (unit_comp != nullptr) {
    auto troop_type_opt =
        Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);

    if (troop_type_opt) {
      const auto &nation_reg = Game::Systems::NationRegistry::instance();
      const Game::Systems::Nation *nation =
          nation_reg.get_nation_for_player(unit_comp->owner_id);
      Game::Systems::NationID nation_id =
          nation != nullptr ? nation->id : nation_reg.default_nation_id();

      const auto profile =
          Game::Systems::TroopProfileService::instance().get_profile(
              nation_id, *troop_type_opt);

      ring_size = profile.visuals.selection_ring_size;
      ring_offset += profile.visuals.selection_ring_y_offset;
      ground_offset = profile.visuals.selection_ring_ground_offset;
    } else {

      auto &config = Game::Units::TroopConfig::instance();
      ring_size = config.getSelectionRingSize(unit_comp->spawn_type);
      ring_offset += config.getSelectionRingYOffset(unit_comp->spawn_type);
      ground_offset =
          config.getSelectionRingGroundOffset(unit_comp->spawn_type);
    }
  }
  if (transform != nullptr) {
    scale_y = transform->scale.y;
  }

  QVector3D pos(transform->position.x, transform->position.y,
                transform->position.z);
  auto &terrain_service = Game::Map::TerrainService::instance();
  float terrain_y = transform->position.y;
  if (terrain_service.is_initialized()) {
    terrain_y = terrain_service.get_terrain_height(pos.x(), pos.z());
  } else {
    terrain_y -= ground_offset * scale_y;
  }
  pos.setY(terrain_y);

  QMatrix4x4 ring_model;
  ring_model.translate(pos.x(), pos.y() + ring_offset, pos.z());
  ring_model.scale(ring_size, 1.0F, ring_size);

  if (selected) {
    selection_ring(ring_model, 0.6F, 0.25F, QVector3D(0.2F, 0.4F, 1.0F));
  } else if (hovered) {
    selection_ring(ring_model, 0.35F, 0.15F, QVector3D(0.90F, 0.90F, 0.25F));
  }
}

void Renderer::enqueue_mode_indicator(
    Engine::Core::TransformComponent *transform,
    Engine::Core::UnitComponent *unit_comp, bool has_attack,
    bool has_guard_mode, bool has_hold_mode, bool has_patrol) {
  if (transform == nullptr) {
    return;
  }

  if (!has_attack && !has_guard_mode && !has_hold_mode && !has_patrol) {
    return;
  }

  float indicator_height = Render::Geom::k_indicator_height_base;
  float indicator_size = Render::Geom::k_indicator_size;

  if (unit_comp != nullptr) {
    auto troop_type_opt =
        Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);

    if (troop_type_opt) {
      const auto &nation_reg = Game::Systems::NationRegistry::instance();
      const Game::Systems::Nation *nation =
          nation_reg.get_nation_for_player(unit_comp->owner_id);
      Game::Systems::NationID nation_id =
          nation != nullptr ? nation->id : nation_reg.default_nation_id();

      const auto profile =
          Game::Systems::TroopProfileService::instance().get_profile(
              nation_id, *troop_type_opt);

      indicator_height += profile.visuals.selection_ring_y_offset *
                          Render::Geom::k_indicator_height_multiplier;
    }
  }

  if (transform != nullptr) {
    indicator_height *= transform->scale.y;
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
      if (ndc_x < -margin || ndc_x > margin || ndc_y < -margin ||
          ndc_y > margin || ndc_z < -1.0F || ndc_z > 1.0F) {
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

    constexpr float k_pi = 3.14159265358979323846F;
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

  mode_indicator(indicator_model, mode_type, color,
                 Render::Geom::k_indicator_alpha);
}

void Renderer::render_world(Engine::Core::World *world) {
  if (m_paused.load()) {
    return;
  }
  if (world == nullptr) {
    return;
  }

  std::lock_guard<std::recursive_mutex> const guard(world->get_entity_mutex());

  auto &vis = Game::Map::VisibilityService::instance();
  const bool visibility_enabled = vis.is_initialized();
  Game::Map::VisibilityService::Snapshot visibility_snapshot;
  if (visibility_enabled) {
    visibility_snapshot = vis.snapshot();
  }

  auto renderable_entities =
      world->get_entities_with<Engine::Core::RenderableComponent>();

  const auto &gfxSettings = Render::GraphicsSettings::instance();
  const auto &batch_config = gfxSettings.batching_config();

  float cameraHeight = 0.0F;
  if (m_camera != nullptr) {
    cameraHeight = m_camera->get_position().y();
  }

  ++m_frame_counter;

  int visibleUnitCount = 0;
  std::vector<UnitRenderEntry> unit_entries;
  std::vector<RenderEntry> building_entries;
  std::vector<RenderEntry> other_entries;
  unit_entries.reserve(renderable_entities.size());
  building_entries.reserve(renderable_entities.size());
  other_entries.reserve(renderable_entities.size());

  for (auto *entity : renderable_entities) {
    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    uint32_t const entity_id = entity->get_id();

    auto *unit_comp = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp != nullptr) && unit_comp->health <= 0) {
      continue;
    }

    if (unit_comp != nullptr) {

      auto &cached =
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

      bool const is_selected =
          (m_selected_ids.find(entity_id) != m_selected_ids.end());
      bool const is_hovered = (entity_id == m_hovered_entity_id);
      entry.selected = is_selected;
      entry.hovered = is_hovered;
      entry.renderer_key = cached.renderer_key;
      entry.movement = cached.movement;
      entry.moving = is_unit_moving(entry.movement);

      UnitRenderCache::update_model_matrix(cached);
      entry.model_matrix = cached.model_matrix;

      if (m_camera != nullptr) {
        QVector3D const unit_pos(cached.transform->position.x,
                                 cached.transform->position.y,
                                 cached.transform->position.z);
        if (m_camera->is_in_frustum(unit_pos, 4.0F)) {
          ++visibleUnitCount;
        }

        float const cull_radius = get_unit_cull_radius(unit_comp->spawn_type);
        entry.in_frustum = m_camera->is_in_frustum(unit_pos, cull_radius);

        QVector3D const cam_pos = m_camera->get_position();
        float const dx = unit_pos.x() - cam_pos.x();
        float const dz = unit_pos.z() - cam_pos.z();
        entry.distance_sq = dx * dx + dz * dz;
      }

      if (unit_comp->owner_id != m_local_owner_id && visibility_enabled) {
        entry.fog_visible = visibility_snapshot.isVisibleWorld(
            cached.transform->position.x, cached.transform->position.z);
      }

      auto *attack_comp =
          entity->get_component<Engine::Core::AttackComponent>();
      entry.has_attack = (attack_comp != nullptr) && attack_comp->in_melee_lock;

      auto *guard_mode =
          entity->get_component<Engine::Core::GuardModeComponent>();
      entry.has_guard_mode = (guard_mode != nullptr) && guard_mode->active;

      auto *hold_mode =
          entity->get_component<Engine::Core::HoldModeComponent>();
      entry.has_hold_mode = (hold_mode != nullptr) && hold_mode->active;

      auto *patrol_comp =
          entity->get_component<Engine::Core::PatrolComponent>();
      entry.has_patrol = (patrol_comp != nullptr) && patrol_comp->patrolling;

      unit_entries.push_back(std::move(entry));
      continue;
    }

    auto *renderable =
        entity->get_component<Engine::Core::RenderableComponent>();
    if ((renderable == nullptr) || !renderable->visible) {
      continue;
    }

    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
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
      entry.renderer_key = renderable->renderer_id;
    }

    auto *building_comp =
        entity->get_component<Engine::Core::BuildingComponent>();
    if (building_comp != nullptr) {
      building_entries.push_back(std::move(entry));
    } else {
      other_entries.push_back(std::move(entry));
    }
  }

  m_unit_render_cache.prune(m_frame_counter);
  m_model_matrix_cache.prune(m_frame_counter);

  auto &battleOptimizer = Render::BattleRenderOptimizer::instance();
  battleOptimizer.set_visible_unit_count(visibleUnitCount);
  uint32_t optimizer_frame = battleOptimizer.frame_counter();

  float batching_ratio =
      gfxSettings.calculate_batching_ratio(visibleUnitCount, cameraHeight);

  float batching_boost = battleOptimizer.get_batching_boost();
  batching_ratio = std::min(1.0F, batching_ratio * batching_boost);

  PrimitiveBatcher batcher;
  if (batching_ratio > 0.0F) {
    batcher.reserve(2000, 4000, 500);
  }

  float fullShaderMaxDistance = 30.0F * (1.0F - batching_ratio * 0.7F);
  if (batch_config.force_batching) {
    fullShaderMaxDistance = 0.0F;
  }
  float fullShaderMaxDistanceSq = fullShaderMaxDistance * fullShaderMaxDistance;

  BatchingSubmitter batchSubmitter(this, &batcher);

  ResourceManager *res = resources();

  auto resolve_fallback_mesh =
      [&](Engine::Core::RenderableComponent *renderable) -> Mesh * {
    Mesh *mesh_to_draw = nullptr;
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

  auto draw_contact_shadow = [&](Engine::Core::TransformComponent *transform,
                                 Engine::Core::UnitComponent *unit_comp) {
    if (res == nullptr) {
      return;
    }
    Mesh *contact_quad = res->quad();
    Texture *white = res->white();
    if ((contact_quad == nullptr) || (white == nullptr)) {
      return;
    }
    QMatrix4x4 contact_base;
    contact_base.translate(transform->position.x, transform->position.y + 0.03F,
                           transform->position.z);
    constexpr float k_contact_shadow_rotation = -90.0F;
    contact_base.rotate(k_contact_shadow_rotation, 1.0F, 0.0F, 0.0F);
    float const footprint =
        std::max({transform->scale.x, transform->scale.z, 0.6F});

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

    QMatrix4x4 c0 = contact_base;
    c0.scale(base_scale_x * 0.60F, base_scale_y * 0.60F, 1.0F);
    mesh(contact_quad, c0, col, white, center_alpha);

    QMatrix4x4 c1 = contact_base;
    c1.scale(base_scale_x * 0.95F, base_scale_y * 0.95F, 1.0F);
    mesh(contact_quad, c1, col, white, mid_alpha);

    QMatrix4x4 c2 = contact_base;
    c2.scale(base_scale_x * 1.35F, base_scale_y * 1.35F, 1.0F);
    mesh(contact_quad, c2, col, white, outer_alpha);
  };

  for (auto &entry : unit_entries) {
    if (!entry.in_frustum || !entry.fog_visible) {
      continue;
    }

    bool should_update_temporal = battleOptimizer.should_render_unit(
        entry.entity_id, entry.moving, entry.selected, entry.hovered);

    const QMatrix4x4 &model_matrix = entry.model_matrix;

    bool drawn_by_registry = false;
    if (m_entity_registry) {
      auto fn = m_entity_registry->get(entry.renderer_key);
      if (fn) {
        DrawContext ctx{resources(), entry.entity, world, model_matrix};

        ctx.selected = entry.selected;
        ctx.hovered = entry.hovered;
        bool should_update_animation = true;
        if (should_update_temporal) {
          should_update_animation = battleOptimizer.should_update_animation(
              entry.entity_id, entry.distance_sq, entry.selected);
        } else {
          should_update_animation = false;
        }

        float animation_time =
            resolve_animation_time(entry.entity_id, should_update_animation,
                                   m_accumulated_time, optimizer_frame);

        ctx.animation_time = animation_time;
        ctx.renderer_id = entry.renderer_key;
        ctx.backend = m_backend.get();
        ctx.camera = m_camera;
        ctx.animation_throttled = !should_update_animation;

        bool useBatching = (batching_ratio > 0.0F) &&
                           (entry.distance_sq > fullShaderMaxDistanceSq) &&
                           !entry.selected && !entry.hovered &&
                           !batch_config.never_batch;

        if (useBatching) {
          fn(ctx, batchSubmitter);
        } else {
          fn(ctx, *this);
        }

        drawn_by_registry = true;
      }
    }
    if (drawn_by_registry) {
      if (entry.selected || entry.hovered) {
        enqueue_selection_ring(entry.entity, entry.transform, entry.unit,
                               entry.selected, entry.hovered);
      }
      enqueue_mode_indicator(entry.transform, entry.unit, entry.has_attack,
                             entry.has_guard_mode, entry.has_hold_mode,
                             entry.has_patrol);
      continue;
    }

    Mesh *mesh_to_draw = resolve_fallback_mesh(entry.renderable);
    QVector3D const color =
        QVector3D(entry.renderable->color[0], entry.renderable->color[1],
                  entry.renderable->color[2]);

    draw_contact_shadow(entry.transform, entry.unit);
    if (entry.selected || entry.hovered) {
      enqueue_selection_ring(entry.entity, entry.transform, entry.unit,
                             entry.selected, entry.hovered);
    }
    enqueue_mode_indicator(entry.transform, entry.unit, entry.has_attack,
                           entry.has_guard_mode, entry.has_hold_mode,
                           entry.has_patrol);
    mesh(mesh_to_draw, model_matrix, color,
         (res != nullptr) ? res->white() : nullptr, 1.0F);
  }

  auto render_non_unit_entry = [&](const RenderEntry &entry) {
    const QMatrix4x4 &model_matrix = m_model_matrix_cache.get_or_create(
        entry.entity_id, entry.transform, m_frame_counter);

    bool drawn_by_registry = false;
    if (m_entity_registry && !entry.renderer_key.empty()) {
      auto fn = m_entity_registry->get(entry.renderer_key);
      if (fn) {
        DrawContext ctx{resources(), entry.entity, world, model_matrix};
        ctx.selected = entry.selected;
        ctx.hovered = entry.hovered;
        ctx.animation_time = m_accumulated_time;
        ctx.renderer_id = entry.renderer_key;
        ctx.backend = m_backend.get();
        ctx.camera = m_camera;
        ctx.animation_throttled = false;
        fn(ctx, *this);
        drawn_by_registry = true;
      }
    }
    if (drawn_by_registry) {
      if (entry.selected || entry.hovered) {
        enqueue_selection_ring(entry.entity, entry.transform, entry.unit,
                               entry.selected, entry.hovered);
      }
      return;
    }

    Mesh *mesh_to_draw = resolve_fallback_mesh(entry.renderable);
    QVector3D const color =
        QVector3D(entry.renderable->color[0], entry.renderable->color[1],
                  entry.renderable->color[2]);

    draw_contact_shadow(entry.transform, entry.unit);
    if (entry.selected || entry.hovered) {
      enqueue_selection_ring(entry.entity, entry.transform, entry.unit,
                             entry.selected, entry.hovered);
    }
    mesh(mesh_to_draw, model_matrix, color,
         (res != nullptr) ? res->white() : nullptr, 1.0F);
  };

  for (const auto &entry : building_entries) {
    render_non_unit_entry(entry);
  }

  for (const auto &entry : other_entries) {
    render_non_unit_entry(entry);
  }

  if ((m_active_queue != nullptr) && batcher.total_count() > 0) {
    PrimitiveBatchParams params;
    params.view_proj = m_view_proj;

    if (batcher.sphere_count() > 0) {
      PrimitiveBatchCmd cmd;
      cmd.type = PrimitiveType::Sphere;
      cmd.instances = batcher.sphere_data();
      cmd.params = params;
      m_active_queue->submit(std::move(cmd));
    }

    if (batcher.cylinder_count() > 0) {
      PrimitiveBatchCmd cmd;
      cmd.type = PrimitiveType::Cylinder;
      cmd.instances = batcher.cylinder_data();
      cmd.params = params;
      m_active_queue->submit(std::move(cmd));
    }

    if (batcher.cone_count() > 0) {
      PrimitiveBatchCmd cmd;
      cmd.type = PrimitiveType::Cone;
      cmd.instances = batcher.cone_data();
      cmd.params = params;
      m_active_queue->submit(std::move(cmd));
    }
  }

  render_construction_previews(world, vis, visibility_enabled);
}

void Renderer::prewarm_unit_templates(
    Engine::Core::World *world,
    TemplatePrewarmProgressCallback progress_callback) {
  cancel_async_template_prewarm();
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

  if (!report_progress(TemplatePrewarmProgress::Phase::CollectingProfiles, 0,
                       0)) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, 0, 0);
    return;
  }

  struct PrewarmProfile {
    std::string renderer_id;
    Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
    Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
    int max_health{1};
    bool is_mounted{false};
    bool is_elephant{false};
    RenderFunc fn;
  };

  struct PrewarmProfileKey {
    std::string renderer_id;
    Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
    Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
    bool operator==(const PrewarmProfileKey &other) const {
      return renderer_id == other.renderer_id &&
             spawn_type == other.spawn_type && nation_id == other.nation_id;
    }
  };

  struct PrewarmProfileKeyHash {
    std::size_t operator()(const PrewarmProfileKey &key) const noexcept {
      std::size_t h = std::hash<std::string>()(key.renderer_id);
      h ^= static_cast<std::size_t>(key.spawn_type) + 0x9e3779b9 + (h << 6) +
           (h >> 2);
      h ^= static_cast<std::size_t>(static_cast<std::uint8_t>(key.nation_id)) +
           0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };

  struct PrewarmWorkItem {
    std::size_t profile_index{0};
    int owner_id{0};
    HumanoidLOD lod{HumanoidLOD::Full};
    std::uint8_t variant{0};
    AnimKey anim_key{};
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
      return 60'000;
    case GraphicsQuality::Medium:
      return 100'000;
    case GraphicsQuality::High:
      return 160'000;
    case GraphicsQuality::Ultra:
    default:
      return 240'000;
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

  auto add_profile = [&](const std::string &renderer_id,
                         Game::Units::SpawnType spawn_type,
                         Game::Systems::NationID nation_id,
                         int max_health) {
    if (!is_prewarmable_spawn(spawn_type) || renderer_id.empty()) {
      return;
    }
    PrewarmProfileKey key{renderer_id, spawn_type, nation_id};
    if (!profile_seen.insert(key).second) {
      return;
    }
    auto fn = m_entity_registry->get(renderer_id);
    if (!fn) {
      return;
    }

    PrewarmProfile p;
    p.renderer_id = renderer_id;
    p.spawn_type = spawn_type;
    p.nation_id = nation_id;
    p.max_health = std::max(1, max_health);
    p.is_elephant = (spawn_type == Game::Units::SpawnType::Elephant);
    p.is_mounted = (spawn_type == Game::Units::SpawnType::MountedKnight ||
                    spawn_type == Game::Units::SpawnType::HorseArcher ||
                    spawn_type == Game::Units::SpawnType::HorseSpearman);
    p.fn = std::move(fn);
    profiles.push_back(std::move(p));
  };

  if (world != nullptr) {
    auto world_units = world->get_entities_with<Engine::Core::UnitComponent>();
    for (auto *entity : world_units) {
      if (entity == nullptr ||
          entity->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      auto *unit = entity->get_component<Engine::Core::UnitComponent>();
      auto *renderable =
          entity->get_component<Engine::Core::RenderableComponent>();
      if (unit == nullptr || renderable == nullptr ||
          unit->health <= 0 || renderable->renderer_id.empty()) {
        continue;
      }

      if (!is_prewarmable_spawn(unit->spawn_type)) {
        continue;
      }

      add_owner(unit->owner_id);
      active_nation_ids.insert(unit->nation_id);
      add_profile(renderable->renderer_id, unit->spawn_type, unit->nation_id,
                  unit->max_health);
    }
  }

  if (owner_ids.empty()) {
    const auto &owners =
        Game::Systems::OwnerRegistry::instance().get_all_owners();
    for (const auto &owner : owners) {
      add_owner(owner.owner_id);
    }
  }
  if (owner_ids.empty()) {
    add_owner(0);
  }

  const auto &troops = Game::Units::TroopCatalog::instance().get_all_classes();
  const auto &nations =
      Game::Systems::NationRegistry::instance().get_all_nations();
  const bool restrict_to_active_nations = !active_nation_ids.empty();

  for (const auto &nation : nations) {
    if (restrict_to_active_nations &&
        (active_nation_ids.find(nation.id) == active_nation_ids.end())) {
      continue;
    }
    for (const auto &entry : troops) {
      auto type = entry.first;
      if (!is_prewarmable_troop(type)) {
        continue;
      }

      auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          nation.id, type);
      if (profile.visuals.renderer_id.empty()) {
        continue;
      }

      add_profile(profile.visuals.renderer_id,
                  Game::Units::spawn_typeFromTroopType(type), nation.id,
                  profile.combat.max_health);
    }
  }

  if (profiles.empty()) {
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  auto profile_priority = [](const PrewarmProfile &profile) -> int {
    if (profile.is_elephant) {
      return 0;
    }
    if (profile.is_mounted) {
      return 1;
    }
    return 2;
  };
  std::stable_sort(profiles.begin(), profiles.end(),
                   [&](const PrewarmProfile &lhs, const PrewarmProfile &rhs) {
                     const int lp = profile_priority(lhs);
                     const int rp = profile_priority(rhs);
                     if (lp != rp) {
                       return lp < rp;
                     }
                     return lhs.renderer_id < rhs.renderer_id;
                   });

  std::vector<AnimKey> core_anim_keys;
  std::vector<AnimKey> full_anim_keys;
  core_anim_keys.reserve(192);
  full_anim_keys.reserve(1024);

  auto push_anim_key = [](std::vector<AnimKey> &keys, AnimState state,
                          CombatAnimPhase phase, std::uint8_t frame,
                          std::uint8_t attack_variant) {
    AnimKey key{};
    key.state = state;
    key.combat_phase = phase;
    key.frame = frame;
    key.attack_variant = attack_variant;
    keys.push_back(key);
  };

  auto add_state_frames = [&](std::vector<AnimKey> &keys, AnimState state,
                              int frame_step) {
    const int step = std::max(1, frame_step);
    for (int frame = 0; frame < static_cast<int>(k_anim_frame_count);
         frame += step) {
      push_anim_key(keys, state, CombatAnimPhase::Idle,
                    static_cast<std::uint8_t>(frame), 0);
    }
  };

  auto add_attack_frames =
      [&](std::vector<AnimKey> &keys, AnimState state, int frame_step) {
        constexpr CombatAnimPhase phases[] = {
            CombatAnimPhase::Idle,      CombatAnimPhase::Advance,
            CombatAnimPhase::WindUp,    CombatAnimPhase::Strike,
            CombatAnimPhase::Impact,    CombatAnimPhase::Recover,
            CombatAnimPhase::Reposition};
        const int step = std::max(1, frame_step);
        for (std::uint8_t attack_variant = 0; attack_variant < 3;
             ++attack_variant) {
          for (CombatAnimPhase phase : phases) {
            for (int frame = 0; frame < static_cast<int>(k_anim_frame_count);
                 frame += step) {
              push_anim_key(keys, state, phase,
                            static_cast<std::uint8_t>(frame), attack_variant);
            }
          }
        }
      };

  push_anim_key(core_anim_keys, AnimState::Idle, CombatAnimPhase::Idle, 0, 0);
  add_state_frames(core_anim_keys, AnimState::Move, 4);
  add_state_frames(core_anim_keys, AnimState::Run, 4);
  add_state_frames(core_anim_keys, AnimState::Construct, 4);
  add_state_frames(core_anim_keys, AnimState::Heal, 4);
  add_state_frames(core_anim_keys, AnimState::Hit, 4);
  add_attack_frames(core_anim_keys, AnimState::AttackMelee, 4);
  add_attack_frames(core_anim_keys, AnimState::AttackRanged, 4);

  push_anim_key(full_anim_keys, AnimState::Idle, CombatAnimPhase::Idle, 0, 0);
  add_state_frames(full_anim_keys, AnimState::Move, 1);
  add_state_frames(full_anim_keys, AnimState::Run, 1);
  add_state_frames(full_anim_keys, AnimState::Construct, 1);
  add_state_frames(full_anim_keys, AnimState::Heal, 1);
  add_state_frames(full_anim_keys, AnimState::Hit, 1);
  add_attack_frames(full_anim_keys, AnimState::AttackMelee, 1);
  add_attack_frames(full_anim_keys, AnimState::AttackRanged, 1);

  auto encode_anim_key = [](const AnimKey &key) -> std::uint32_t {
    return static_cast<std::uint32_t>(key.state) |
           (static_cast<std::uint32_t>(key.combat_phase) << 8U) |
           (static_cast<std::uint32_t>(key.frame) << 16U) |
           (static_cast<std::uint32_t>(key.attack_variant) << 24U);
  };

  std::unordered_set<std::uint32_t> core_key_set;
  core_key_set.reserve(core_anim_keys.size() * 2);
  for (const auto &key : core_anim_keys) {
    core_key_set.insert(encode_anim_key(key));
  }

  std::vector<AnimKey> extra_anim_keys;
  extra_anim_keys.reserve(full_anim_keys.size());
  for (const auto &key : full_anim_keys) {
    if (core_key_set.find(encode_anim_key(key)) == core_key_set.end()) {
      extra_anim_keys.push_back(key);
    }
  }

  const std::size_t domain_count = profiles.size() * owner_ids.size() * 3U;
  if (domain_count == 0) {
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  const std::size_t target_template_count = choose_template_budget_by_quality();
  const std::size_t core_anim_count = core_anim_keys.size();
  const std::size_t full_anim_count =
      core_anim_keys.size() + extra_anim_keys.size();

  std::size_t variant_count = k_template_variant_count;
  const std::size_t core_per_variant = domain_count * core_anim_count;
  if (core_per_variant > 0) {
    const std::size_t max_variants_for_core =
        target_template_count / core_per_variant;
    variant_count =
        std::clamp<std::size_t>(max_variants_for_core, 1, k_template_variant_count);
  }

  std::size_t anim_count_budget =
      target_template_count /
      std::max<std::size_t>(1, domain_count * variant_count);
  anim_count_budget = std::max<std::size_t>(anim_count_budget, 1);

  std::size_t anim_count = std::min(full_anim_count, anim_count_budget);
  if (anim_count < core_anim_count && variant_count > 1) {
    variant_count = std::max<std::size_t>(
        1, target_template_count /
               std::max<std::size_t>(1, domain_count * core_anim_count));
    variant_count = std::min<std::size_t>(variant_count, k_template_variant_count);
    anim_count_budget = target_template_count /
                        std::max<std::size_t>(1, domain_count * variant_count);
    anim_count_budget = std::max<std::size_t>(anim_count_budget, 1);
    anim_count = std::min(full_anim_count, anim_count_budget);
  }

  std::vector<std::uint8_t> variant_values;
  variant_values.reserve(variant_count);
  for (std::size_t i = 0; i < variant_count; ++i) {
    std::size_t idx = (i * k_template_variant_count) / variant_count;
    if (idx >= k_template_variant_count) {
      idx = k_template_variant_count - 1;
    }
    variant_values.push_back(static_cast<std::uint8_t>(idx));
  }

  std::vector<AnimKey> selected_core_anim_keys;
  selected_core_anim_keys.reserve(core_anim_count);
  const std::size_t core_take = std::min(core_anim_count, anim_count);
  selected_core_anim_keys.insert(
      selected_core_anim_keys.end(), core_anim_keys.begin(),
      core_anim_keys.begin() + static_cast<std::ptrdiff_t>(core_take));

  std::vector<AnimKey> selected_extra_anim_keys;
  if (anim_count > core_take) {
    const std::size_t extra_take =
        std::min<std::size_t>(anim_count - core_take, extra_anim_keys.size());
    selected_extra_anim_keys.insert(
        selected_extra_anim_keys.end(), extra_anim_keys.begin(),
        extra_anim_keys.begin() + static_cast<std::ptrdiff_t>(extra_take));
  }

  if (selected_core_anim_keys.empty() || variant_values.empty()) {
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  const std::size_t selected_anim_total =
      selected_core_anim_keys.size() + selected_extra_anim_keys.size();
  const std::size_t expected_template_count =
      domain_count * variant_values.size() * selected_anim_total;
  constexpr std::size_t k_cache_min_cap = 50'000;
  constexpr std::size_t k_cache_hard_cap = 300'000;
  const std::size_t cache_entry_cap = std::clamp<std::size_t>(
      expected_template_count +
          std::max<std::size_t>(4096, expected_template_count / 8),
      k_cache_min_cap, k_cache_hard_cap);

  TemplateCache::instance().set_max_entries(cache_entry_cap);
  TemplateCache::instance().clear();
  clear_humanoid_caches();
  PosePaletteCache::instance().generate();

  auto build_work_items =
      [&](const std::vector<AnimKey> &anim_keys) -> std::vector<PrewarmWorkItem> {
    std::vector<PrewarmWorkItem> items;
    items.reserve(domain_count * variant_values.size() * anim_keys.size());
    constexpr HumanoidLOD lods[] = {HumanoidLOD::Full, HumanoidLOD::Reduced,
                                    HumanoidLOD::Minimal};
    for (std::size_t profile_idx = 0; profile_idx < profiles.size();
         ++profile_idx) {
      for (int owner_id : owner_ids) {
        for (HumanoidLOD lod : lods) {
          for (std::uint8_t variant : variant_values) {
            for (const auto &anim_key : anim_keys) {
              PrewarmWorkItem item;
              item.profile_index = profile_idx;
              item.owner_id = owner_id;
              item.lod = lod;
              item.variant = variant;
              item.anim_key = anim_key;
              items.push_back(item);
            }
          }
        }
      }
    }
    return items;
  };

  std::vector<PrewarmWorkItem> core_work_items =
      build_work_items(selected_core_anim_keys);
  std::vector<PrewarmWorkItem> extended_work_items =
      build_work_items(selected_extra_anim_keys);

  const std::size_t total_work_count =
      core_work_items.size() + extended_work_items.size();
  if (core_work_items.empty()) {
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0,
                    total_work_count);
    return;
  }

  if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates, 0,
                       core_work_items.size())) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, 0,
                    total_work_count);
    return;
  }

  std::vector<std::mutex> profile_mutexes(profiles.size());

  const unsigned hw_threads = std::max(1U, std::thread::hardware_concurrency());
  std::size_t worker_count = std::min<std::size_t>(4U, hw_threads);
  if (core_work_items.size() < 20'000) {
    worker_count = 1;
  }

  std::atomic<std::size_t> next_index{0};
  std::atomic<std::size_t> completed_count{0};
  std::atomic<bool> cancel_requested{false};

  auto worker = [&]() {
    TemplateRecorder recorder;
    while (true) {
      if (cancel_requested.load(std::memory_order_relaxed)) {
        break;
      }
      const std::size_t idx =
          next_index.fetch_add(1, std::memory_order_relaxed);
      if (idx >= core_work_items.size()) {
        break;
      }

      const PrewarmWorkItem &item = core_work_items[idx];
      const PrewarmProfile &profile = profiles[item.profile_index];

      Engine::Core::Entity entity(1);
      auto *unit = entity.add_component<Engine::Core::UnitComponent>();
      unit->spawn_type = profile.spawn_type;
      unit->owner_id = item.owner_id;
      unit->nation_id = profile.nation_id;
      unit->max_health = profile.max_health;
      unit->health = profile.max_health;

      auto *transform = entity.add_component<Engine::Core::TransformComponent>();
      transform->position = {0.0F, 0.0F, 0.0F};
      transform->rotation = {0.0F, 0.0F, 0.0F};
      transform->scale = {1.0F, 1.0F, 1.0F};

      auto *renderable =
          entity.add_component<Engine::Core::RenderableComponent>("", "");
      renderable->renderer_id = profile.renderer_id;
      renderable->visible = true;
      const QVector3D team_color =
          Game::Visuals::team_colorForOwner(item.owner_id);
      renderable->color[0] = team_color.x();
      renderable->color[1] = team_color.y();
      renderable->color[2] = team_color.z();

      DrawContext ctx{resources(), &entity, nullptr, QMatrix4x4()};
      ctx.renderer_id = profile.renderer_id;
      ctx.backend = m_backend.get();
      ctx.camera = nullptr;
      ctx.allow_template_cache = true;
      ctx.template_prewarm = true;
      ctx.has_variant_override = true;
      ctx.variant_override = item.variant;
      ctx.force_humanoid_lod = true;
      ctx.forced_humanoid_lod = item.lod;
      ctx.force_horse_lod = profile.is_mounted || profile.is_elephant;
      if (ctx.force_horse_lod) {
        ctx.forced_horse_lod = static_cast<HorseLOD>(item.lod);
      }

      AnimationInputs anim = make_animation_inputs(item.anim_key);
      ctx.animation_override = &anim;
      const bool attack_state =
          (item.anim_key.state == AnimState::AttackMelee) ||
          (item.anim_key.state == AnimState::AttackRanged);
      ctx.has_attack_variant_override = attack_state;
      ctx.attack_variant_override = item.anim_key.attack_variant;

      recorder.reset();
      std::lock_guard<std::mutex> profile_lock(
          profile_mutexes[item.profile_index]);
      profile.fn(ctx, recorder);
      completed_count.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> workers;
  workers.reserve(worker_count);
  for (std::size_t i = 0; i < worker_count; ++i) {
    workers.emplace_back(worker);
  }

  constexpr std::size_t k_progress_report_step = 2048;
  std::size_t last_reported = 0;
  while (!cancel_requested.load(std::memory_order_relaxed)) {
    const std::size_t done =
        std::min(completed_count.load(std::memory_order_relaxed),
                 core_work_items.size());
    if (done >= core_work_items.size()) {
      break;
    }

    if ((done - last_reported) >= k_progress_report_step) {
      last_reported = done;
      if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                           done, core_work_items.size())) {
        cancel_requested.store(true, std::memory_order_relaxed);
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  for (auto &t : workers) {
    t.join();
  }

  const std::size_t core_done =
      std::min(completed_count.load(std::memory_order_relaxed),
               core_work_items.size());
  if (cancel_requested.load(std::memory_order_relaxed) ||
      core_done < core_work_items.size()) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, core_done,
                    total_work_count);
    return;
  }
  if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                       core_done, core_work_items.size())) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, core_done,
                    total_work_count);
    return;
  }

  if (!extended_work_items.empty()) {
    if (!report_progress(
            TemplatePrewarmProgress::Phase::QueueingExtendedTemplates,
            extended_work_items.size(), extended_work_items.size())) {
      report_progress(TemplatePrewarmProgress::Phase::Cancelled, core_done,
                      total_work_count);
      return;
    }

    auto async_state = std::make_shared<AsyncTemplatePrewarmState>();
    async_state->profiles.reserve(profiles.size());
    for (const auto &profile : profiles) {
      AsyncPrewarmProfile async_profile;
      async_profile.renderer_id = profile.renderer_id;
      async_profile.spawn_type = static_cast<int>(profile.spawn_type);
      async_profile.nation_id = static_cast<int>(profile.nation_id);
      async_profile.max_health = profile.max_health;
      async_profile.is_mounted = profile.is_mounted;
      async_profile.is_elephant = profile.is_elephant;
      async_state->profiles.push_back(std::move(async_profile));
    }

    async_state->work_items.reserve(extended_work_items.size());
    for (const auto &item : extended_work_items) {
      AsyncPrewarmWorkItem async_item;
      async_item.profile_index = item.profile_index;
      async_item.owner_id = item.owner_id;
      async_item.lod = static_cast<std::uint8_t>(item.lod);
      async_item.variant = item.variant;
      async_item.anim_state = static_cast<std::uint8_t>(item.anim_key.state);
      async_item.combat_phase =
          static_cast<std::uint8_t>(item.anim_key.combat_phase);
      async_item.frame = item.anim_key.frame;
      async_item.attack_variant = item.anim_key.attack_variant;
      async_state->work_items.push_back(async_item);
    }

    {
      std::lock_guard<std::mutex> lock(m_async_prewarm_mutex);
      m_async_prewarm_state = std::move(async_state);
    }
  }

  report_progress(TemplatePrewarmProgress::Phase::Completed, core_done,
                  total_work_count);
}

void Renderer::render_construction_previews(
    Engine::Core::World *world, const Game::Map::VisibilityService &vis,
    bool visibility_enabled) {
  if (world == nullptr || m_entity_registry == nullptr) {
    return;
  }

  Game::Map::VisibilityService::Snapshot visibility_snapshot;
  if (visibility_enabled) {
    visibility_snapshot = vis.snapshot();
  }

  auto builders =
      world->get_entities_with<Engine::Core::BuilderProductionComponent>();

  for (auto *builder : builders) {
    if (builder->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *builder_prod =
        builder->get_component<Engine::Core::BuilderProductionComponent>();
    auto *transform =
        builder->get_component<Engine::Core::TransformComponent>();
    auto *unit_comp = builder->get_component<Engine::Core::UnitComponent>();

    if (builder_prod == nullptr || transform == nullptr) {
      continue;
    }

    bool show_preview = false;
    float preview_x = transform->position.x;
    float preview_z = transform->position.z;

    if (builder_prod->is_placement_preview &&
        builder_prod->has_construction_site) {
      show_preview = true;
      preview_x = builder_prod->construction_site_x;
      preview_z = builder_prod->construction_site_z;
    } else if (builder_prod->is_placement_preview &&
               builder_prod->in_progress) {
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
          !visibility_snapshot.isVisibleWorld(preview_x, preview_z)) {
        continue;
      }
    }

    if (m_camera != nullptr) {
      QVector3D const pos(preview_x, transform->position.y, preview_z);
      if (!m_camera->is_in_frustum(pos, 5.0F)) {
        continue;
      }
    }

    std::string nation_prefix = "roman";
    if (unit_comp != nullptr) {
      if (unit_comp->nation_id == Game::Systems::NationID::Carthage) {
        nation_prefix = "carthage";
      }
    }

    std::string renderer_key =
        "troops/" + nation_prefix + "/" + builder_prod->product_type;

    auto fn = m_entity_registry->get(renderer_key);
    if (!fn) {
      continue;
    }

    auto &terrain_service = Game::Map::TerrainService::instance();
    const float terrain_height =
        terrain_service.get_terrain_height(preview_x, preview_z);

    QMatrix4x4 model_matrix;
    model_matrix.translate(preview_x, terrain_height, preview_z);

    DrawContext ctx{resources(), builder, world, model_matrix};
    ctx.selected = false;
    ctx.hovered = false;
    ctx.animation_time = m_accumulated_time;
    ctx.renderer_id = renderer_key;
    ctx.backend = m_backend.get();
    ctx.camera = m_camera;

    float const prev_alpha = m_alpha_override;
    m_alpha_override = 0.60F;

    fn(ctx, *this);

    m_alpha_override = prev_alpha;
  }
}

} // namespace Render::GL
