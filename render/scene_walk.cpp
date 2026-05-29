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
#include "profiling/combat_animation_diagnostics.h"
#include "profiling/frame_profile.h"
#include "render_backend_factory.h"
#include "scene_renderer.h"
#include "selection_ring_layout.h"
#include "software_backend.h"
#include "submitter.h"
#include "template_cache.h"
#include "template_prewarm_catalog.h"
#include "visibility_budget.h"
#include "world_chunk.h"

namespace Render::GL {

namespace {
auto render_stage_logging_enabled() -> bool {
  return qEnvironmentVariableIsSet("SOI_RENDER_STAGE_LOG");
}

auto unit_should_emit_rigged_body(Game::Units::SpawnType spawn_type) noexcept -> bool {
  switch (spawn_type) {
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
  case Game::Units::SpawnType::Barracks:
  case Game::Units::SpawnType::DefenseTower:
  case Game::Units::SpawnType::Home:
  case Game::Units::SpawnType::WallSegment:
    return false;
  default:
    return true;
  }
}

class RiggedBodyProbeSubmitter final : public ISubmitter {
public:
  explicit RiggedBodyProbeSubmitter(ISubmitter& inner)
      : m_inner(inner) {}

  [[nodiscard]] auto unwrap_submitter() noexcept -> ISubmitter* override {
    return m_inner.unwrap_submitter();
  }

  [[nodiscard]] auto rigged_body_count() const noexcept -> std::uint32_t {
    return m_rigged_body_count;
  }

  void mesh(Mesh* mesh,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* tex = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override {
    m_inner.mesh(mesh, model, color, tex, alpha, material_id);
  }

  void banner(Mesh* mesh,
              const QMatrix4x4& model,
              const QVector3D& color,
              const QVector3D& trim_color,
              Texture* tex = nullptr,
              float alpha = 1.0F,
              int material_id = 0) override {
    m_inner.banner(mesh, model, color, trim_color, tex, alpha, material_id);
  }

  void part(Mesh* mesh,
            Material* material,
            const QMatrix4x4& model,
            const QVector3D& color,
            Texture* tex = nullptr,
            float alpha = 1.0F,
            int material_id = 0) override {
    m_inner.part(mesh, material, model, color, tex, alpha, material_id);
  }

  void rigged(const RiggedCreatureCmd& cmd) override {
    if (cmd.mesh != nullptr) {
      ++m_rigged_body_count;
    }
    m_inner.rigged(cmd);
  }

  void cylinder(const QVector3D& start,
                const QVector3D& end,
                float radius,
                const QVector3D& color,
                float alpha = 1.0F) override {
    m_inner.cylinder(start, end, radius, color, alpha);
  }

  void selection_ring(const QMatrix4x4& model,
                      float alpha_inner,
                      float alpha_outer,
                      const QVector3D& color) override {
    m_inner.selection_ring(model, alpha_inner, alpha_outer, color);
  }

  void grid(const QMatrix4x4& model,
            const QVector3D& color,
            float cell_size,
            float thickness,
            float extent) override {
    m_inner.grid(model, color, cell_size, thickness, extent);
  }

  void selection_smoke(const QMatrix4x4& model,
                       const QVector3D& color,
                       float base_alpha = 0.15F) override {
    m_inner.selection_smoke(model, color, base_alpha);
  }

  void healing_beam(const QVector3D& start,
                    const QVector3D& end,
                    const QVector3D& color,
                    float progress,
                    float beam_width,
                    float intensity,
                    float time) override {
    m_inner.healing_beam(start, end, color, progress, beam_width, intensity, time);
  }

  void healer_aura(const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time) override {
    m_inner.healer_aura(position, color, radius, intensity, time);
  }

  void combat_dust(const QVector3D& position,
                   const QVector3D& color,
                   float radius,
                   float intensity,
                   float time) override {
    m_inner.combat_dust(position, color, radius, intensity, time);
  }

  void stone_impact(const QVector3D& position,
                    const QVector3D& color,
                    float radius,
                    float intensity,
                    float time) override {
    m_inner.stone_impact(position, color, radius, intensity, time);
  }

  void mode_indicator(const QMatrix4x4& model,
                      int mode_type,
                      const QVector3D& color,
                      float alpha = 1.0F) override {
    m_inner.mode_indicator(model, mode_type, color, alpha);
  }

private:
  ISubmitter& m_inner;
  std::uint32_t m_rigged_body_count{0U};
};

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

} // namespace

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
  auto& frame_profile = Render::Profiling::global_profile();
  frame_profile.visible_soldiers = get_humanoid_render_stats().soldiers_rendered;
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

        const bool batching_available =
            !m_force_full_creature_lod && !entry.combat_active && batching_ratio > 0.0F;
        const auto tier = m_force_full_creature_lod
                              ? Render::Pipeline::LodTier::Full
                              : Render::Pipeline::select_lod(lod_in);

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

        const bool use_batching =
            batching_available && (tier == Render::Pipeline::LodTier::Simplified ||
                                   tier == Render::Pipeline::LodTier::Minimal);
        tier_is_minimal = tier == Render::Pipeline::LodTier::Minimal;
        RiggedBodyProbeSubmitter probe(use_batching
                                           ? static_cast<ISubmitter&>(batch_submitter)
                                           : static_cast<ISubmitter&>(*this));
        (*fn)(ctx, probe);

        if (entry.unit != nullptr && probe.rigged_body_count() == 0U &&
            unit_should_emit_rigged_body(entry.unit->spawn_type) && !tier_is_minimal) {
          static std::mutex warning_mutex;
          static std::unordered_set<std::string> warned_units;
          const std::string warning_key =
              std::to_string(entry.entity_id) + ":" + entry.renderer_key;
          bool should_warn = false;
          {
            std::lock_guard<std::mutex> const lock(warning_mutex);
            should_warn = warned_units.emplace(warning_key).second;
          }
          if (should_warn) {
            qWarning().noquote()
                << QStringLiteral(
                       "Renderer: unit renderer emitted no rigged body; "
                       "entity=%1 renderer='%2' spawn='%3' selected=%4 hovered=%5 "
                       "combat=%6 distance_sq=%7 batching=%8 lod_tier=%9")
                       .arg(entry.entity_id)
                       .arg(QString::fromStdString(entry.renderer_key))
                       .arg(Game::Units::spawn_typeToQString(entry.unit->spawn_type))
                       .arg(static_cast<int>(entry.selected))
                       .arg(static_cast<int>(entry.hovered))
                       .arg(static_cast<int>(entry.combat_active))
                       .arg(entry.distance_sq)
                       .arg(static_cast<int>(use_batching))
                       .arg(static_cast<int>(tier));
          }
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

  auto render_preview_like_entity = [&](Engine::Core::Entity* entity,
                                        float alpha_multiplier,
                                        const QVector3D& marker_color,
                                        float marker_alpha,
                                        float progress) {
    if (entity == nullptr) {
      return;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if (transform == nullptr || renderable == nullptr) {
      return;
    }

    const float preview_x = transform->position.x;
    const float preview_z = transform->position.z;

    int preview_owner = m_local_owner_id;
    if (const auto* preview =
            entity->get_component<Engine::Core::ConstructionPreviewComponent>()) {
      preview_owner = preview->owner_id;
    } else if (const auto* site =
                   entity
                       ->get_component<Engine::Core::WallConstructionSiteComponent>()) {
      preview_owner = site->owner_id;
    }

    if (preview_owner != m_local_owner_id && visibility_enabled &&
        !Game::Map::should_render_non_local_preview(
            resolved_visibility_snapshot, preview_x, preview_z)) {
      return;
    }

    if (m_camera != nullptr) {
      QVector3D const pos(preview_x, transform->position.y, preview_z);
      if (!m_camera->is_in_frustum(pos, 5.0F)) {
        return;
      }
    }

    std::string const renderer_key =
        std::string(canonicalize_building_renderer_key(renderable->renderer_id));
    const auto renderer_handle = m_entity_registry->get_handle(renderer_key);
    auto const* fn = m_entity_registry->get(renderer_handle);
    if (fn == nullptr) {
      return;
    }

    QMatrix4x4 model_matrix;
    model_matrix.translate(
        transform->position.x, transform->position.y, transform->position.z);
    model_matrix.rotate(transform->rotation.x, 1.0F, 0.0F, 0.0F);
    model_matrix.rotate(transform->rotation.y, 0.0F, 1.0F, 0.0F);
    model_matrix.rotate(transform->rotation.z, 0.0F, 0.0F, 1.0F);
    model_matrix.scale(transform->scale.x, transform->scale.y, transform->scale.z);

    float preview_distance_sq = 0.0F;
    if (m_camera != nullptr) {
      QVector3D const cam_pos = m_camera->get_position();
      float const dx = transform->position.x - cam_pos.x();
      float const dz = transform->position.z - cam_pos.z();
      preview_distance_sq = dx * dx + dz * dz;
    }

    if (auto* quad = (resources() != nullptr) ? resources()->quad() : nullptr;
        quad != nullptr) {
      QMatrix4x4 marker_model;
      marker_model.translate(
          transform->position.x, transform->position.y + 0.03F, transform->position.z);
      marker_model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
      marker_model.scale(1.15F, 1.15F, 1.0F);
      mesh(quad,
           marker_model,
           marker_color,
           (resources() != nullptr) ? resources()->white() : nullptr,
           marker_alpha);
    }

    DrawContext ctx{resources(), entity, world, model_matrix};
    ctx.selected = false;
    ctx.hovered = false;
    ctx.animation_time = m_accumulated_time;
    ctx.distance_sq = preview_distance_sq;
    ctx.renderer_id = renderer_key;
    ctx.renderer_handle = renderer_handle;
    ctx.backend = m_gl_backend;
    ctx.camera = m_camera;
    ctx.alpha_multiplier = alpha_multiplier;

    (*fn)(ctx, *this);

    if (progress > 0.0F) {
      if (auto* quad = (resources() != nullptr) ? resources()->quad() : nullptr;
          quad != nullptr) {
        QMatrix4x4 bar_bg;
        bar_bg.translate(transform->position.x,
                         transform->position.y + 1.45F,
                         transform->position.z);
        bar_bg.scale(1.25F, 0.08F, 1.0F);
        mesh(quad,
             bar_bg,
             QVector3D(0.08F, 0.08F, 0.08F),
             (resources() != nullptr) ? resources()->white() : nullptr,
             0.70F);

        QMatrix4x4 bar_fill;
        bar_fill.translate(transform->position.x - 0.625F + 0.625F * progress,
                           transform->position.y + 1.46F,
                           transform->position.z);
        bar_fill.scale(1.25F * progress, 0.05F, 1.0F);
        mesh(quad,
             bar_fill,
             QVector3D(0.90F, 0.72F, 0.24F),
             (resources() != nullptr) ? resources()->white() : nullptr,
             0.92F);
      }
    }
  };

  auto preview_entities =
      world->get_entities_with<Engine::Core::ConstructionPreviewComponent>();
  for (auto* entity : preview_entities) {
    auto* preview = entity->get_component<Engine::Core::ConstructionPreviewComponent>();
    if (preview == nullptr) {
      continue;
    }
    render_preview_like_entity(entity,
                               0.62F,
                               preview->valid ? QVector3D(0.24F, 0.75F, 0.30F)
                                              : QVector3D(0.82F, 0.24F, 0.24F),
                               preview->valid ? 0.28F : 0.36F,
                               0.0F);
  }

  auto site_entities =
      world->get_entities_with<Engine::Core::WallConstructionSiteComponent>();
  for (auto* entity : site_entities) {
    auto* site = entity->get_component<Engine::Core::WallConstructionSiteComponent>();
    if (site == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    render_preview_like_entity(
        entity, 0.82F, QVector3D(0.78F, 0.60F, 0.20F), 0.22F, site->progress);
  }
}

} // namespace Render::GL
