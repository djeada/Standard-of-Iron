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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
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

void Renderer::shutdown() { m_backend.reset(); }

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

void Renderer::prewarm_unit_templates() {
  if (!m_entity_registry) {
    return;
  }

  TemplateCache::instance().clear();
  clear_humanoid_caches();
  PosePaletteCache::instance().generate();

  TemplateRecorder recorder;
  recorder.reset();

  std::vector<int> owner_ids;
  const auto &owners =
      Game::Systems::OwnerRegistry::instance().get_all_owners();
  for (const auto &owner : owners) {
    owner_ids.push_back(owner.owner_id);
  }
  if (owner_ids.empty()) {
    owner_ids.push_back(0);
  }

  std::vector<AnimKey> anim_keys;
  anim_keys.reserve(1);

  {
    AnimKey key{};
    key.state = AnimState::Idle;
    key.frame = 0;
    key.combat_phase = CombatAnimPhase::Idle;
    key.attack_variant = 0;
    anim_keys.push_back(key);
  }

  auto is_prewarmeable_troop = [](Game::Units::TroopType type) -> bool {
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

  const auto &troops = Game::Units::TroopCatalog::instance().get_all_classes();
  const auto &nations =
      Game::Systems::NationRegistry::instance().get_all_nations();

  for (const auto &nation : nations) {
    for (const auto &entry : troops) {
      auto type = entry.first;
      if (!is_prewarmeable_troop(type)) {
        continue;
      }

      auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          nation.id, type);
      if (profile.visuals.renderer_id.empty()) {
        continue;
      }

      auto fn = m_entity_registry->get(profile.visuals.renderer_id);
      if (!fn) {
        continue;
      }

      Game::Units::SpawnType spawn_type =
          Game::Units::spawn_typeFromTroopType(type);

      bool is_mounted = false;
      bool is_elephant = (spawn_type == Game::Units::SpawnType::Elephant);
      if (spawn_type == Game::Units::SpawnType::MountedKnight ||
          spawn_type == Game::Units::SpawnType::HorseArcher ||
          spawn_type == Game::Units::SpawnType::HorseSpearman) {
        is_mounted = true;
      }

      for (int owner_id : owner_ids) {
        Engine::Core::Entity entity(1);
        auto *unit = entity.add_component<Engine::Core::UnitComponent>();
        unit->spawn_type = spawn_type;
        unit->owner_id = owner_id;
        unit->nation_id = nation.id;
        unit->max_health = std::max(1, profile.combat.max_health);
        unit->health = unit->max_health;

        auto *transform =
            entity.add_component<Engine::Core::TransformComponent>();
        transform->position = {0.0F, 0.0F, 0.0F};
        transform->rotation = {0.0F, 0.0F, 0.0F};
        transform->scale = {1.0F, 1.0F, 1.0F};

        auto *renderable =
            entity.add_component<Engine::Core::RenderableComponent>("", "");
        renderable->renderer_id = profile.visuals.renderer_id;
        renderable->visible = true;
        QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);
        renderable->color[0] = team_color.x();
        renderable->color[1] = team_color.y();
        renderable->color[2] = team_color.z();

        DrawContext ctx{resources(), &entity, nullptr, QMatrix4x4()};
        ctx.renderer_id = profile.visuals.renderer_id;
        ctx.backend = m_backend.get();
        ctx.camera = nullptr;
        ctx.allow_template_cache = true;
        ctx.template_prewarm = true;
        ctx.has_variant_override = true;
        ctx.force_humanoid_lod = true;
        ctx.force_horse_lod = is_mounted || is_elephant;

        for (HumanoidLOD lod :
             {HumanoidLOD::Full, HumanoidLOD::Reduced, HumanoidLOD::Minimal}) {
          ctx.forced_humanoid_lod = lod;
          if (ctx.force_horse_lod) {
            ctx.forced_horse_lod = static_cast<HorseLOD>(lod);
          }

          for (std::uint8_t variant = 0; variant < k_template_variant_count;
               ++variant) {
            ctx.variant_override = variant;

            for (const auto &anim_key : anim_keys) {
              AnimationInputs anim = make_animation_inputs(anim_key);
              ctx.animation_override = &anim;
              bool attack_state = (anim_key.state == AnimState::AttackMelee) ||
                                  (anim_key.state == AnimState::AttackRanged);
              ctx.has_attack_variant_override = attack_state;
              ctx.attack_variant_override = anim_key.attack_variant;
              fn(ctx, recorder);
            }
          }
        }
      }
    }
  }
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
