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
#include <span>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "../game/map/render_visibility_rules.h"
#include "../game/map/terrain_service.h"
#include "../game/map/visibility_service.h"
#include "../game/systems/formation_combat_geometry.h"
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
#if defined(SOI_ENABLE_RUNTIME_TRACING)
auto render_stage_logging_enabled() -> bool {
  return qEnvironmentVariableIsSet("SOI_RENDER_STAGE_LOG");
}
#endif

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

float get_unit_base_cull_radius(Game::Units::SpawnType spawn_type) {
  switch (spawn_type) {
  case Game::Units::SpawnType::MountedKnight:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::HorseSpearman:
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return 4.0F;
  case Game::Units::SpawnType::Elephant:
  case Game::Units::SpawnType::DefenseTower:
    return 5.0F;
  case Game::Units::SpawnType::Barracks:
  case Game::Units::SpawnType::Home:
    return 8.0F;
  case Game::Units::SpawnType::WallSegment:
    return 3.5F;
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::Knight:
    return 2.5F;
  default:
    return 3.0F;
  }
}

auto is_formation_render_spawn(Game::Units::SpawnType spawn_type) noexcept -> bool {
  using Game::Units::SpawnType;
  switch (spawn_type) {
  case SpawnType::Spearman:
  case SpawnType::Archer:
  case SpawnType::Knight:
  case SpawnType::MountedKnight:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
    return true;
  default:
    return false;
  }
}

float get_unit_cull_radius(const Engine::Core::UnitComponent& unit) {
  const float base_radius = get_unit_base_cull_radius(unit.spawn_type);
  if (!is_formation_render_spawn(unit.spawn_type)) {
    return base_radius;
  }

  const auto definition = Game::Systems::FormationCombat::resolve_definition(unit);
  const int columns = std::clamp(definition.max_per_row, 1, definition.total_count);
  const int rows = (definition.total_count + columns - 1) / columns;
  const float half_width = 0.5F * static_cast<float>(columns - 1) * definition.spacing;
  const float half_depth = 0.5F * static_cast<float>(rows - 1) * definition.spacing;
  const float body_padding =
      definition.category == Game::Systems::FormationUnitCategory::Cavalry ? 2.75F
                                                                           : 1.75F;
  const float formation_radius = std::hypot(half_width, half_depth) + body_padding;
  return std::max(base_radius, formation_radius);
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit_comp)
    -> int {
  return Game::Systems::FormationCombat::resolve_definition(unit_comp).total_count;
}

auto is_unit_combat_active(
    const Engine::Core::CreaturePresentationComponent* presentation) -> bool {
  return presentation != nullptr && presentation->snapshot_valid &&
         presentation->combat_active;
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
  Render::CachedUnitData* cache{nullptr};
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  Engine::Core::MotionPresentationComponent* motion{nullptr};
  std::string renderer_key;
  Render::GL::RendererHandle renderer_handle{Render::GL::k_invalid_renderer_handle};
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
  float distance_sq{0.0F};
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
      const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          unit_comp->nation_id, *troop_type_opt);
      auto const formation =
          Game::Systems::FormationCombat::resolve_definition(*unit_comp);
      int const individuals_per_unit = resolved_individuals_per_unit(*unit_comp);

      ring_size =
          Detail::selection_ring_visual_size(unit_comp->spawn_type,
                                             individuals_per_unit,
                                             profile.visuals.selection_ring_size,
                                             formation.spacing);
      ground_offset = profile.visuals.selection_ring_ground_offset;

      auto const* formation_presentation =
          entity != nullptr
              ? entity->get_component<Engine::Core::FormationPresentationComponent>()
              : nullptr;
      std::span<const Engine::Core::FormationSoldierPresentation> soldiers;
      if (formation_presentation != nullptr) {
        soldiers = formation_presentation->soldiers;
      }

      placements = build_selection_ring_layout(
          {.soldiers = soldiers,
           .ring_size = ring_size,
           .position = QVector3D(
               transform->position.x, transform->position.y, transform->position.z),
           .yaw_degrees = transform->rotation.y});
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

void Renderer::enqueue_mode_indicator(std::uint32_t entity_id,
                                      Engine::Core::TransformComponent* transform,
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
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  Render::Profiling::CombatAnimationDiagnostics::instance().record_mode_indicator(
      entity_id);
#else
  (void)entity_id;
#endif
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
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    log_render_first_use_once(
        "render-registry-attach",
        QStringLiteral("attached persistent render registry to world"));
#endif
  }

  auto& vis = Game::Map::VisibilityService::instance();
  const bool visibility_enabled = vis.is_initialized();
  const auto& unit_ids = m_render_registry.unit_ids();
  const auto& building_ids = m_render_registry.building_ids();
  const auto& other_ids = m_render_registry.other_ids();

  const auto& gfx_settings = Render::GraphicsSettings::instance();
  const auto& batch_config = gfx_settings.batching_config();
  const bool full_creature_detail =
      m_force_full_creature_lod || !gfx_settings.creature_lod_enabled();

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
    auto const* creature_presentation =
        entity->get_component<Engine::Core::CreaturePresentationComponent>();
    bool const has_death_motion =
        creature_presentation != nullptr && creature_presentation->snapshot_valid &&
        (creature_presentation->is_dying || creature_presentation->is_dead);
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
      entry.cache = &cached;
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

      QVector3D const unit_pos(cached.transform->position.x,
                               cached.transform->position.y,
                               cached.transform->position.z);
      float const cull_radius = get_unit_cull_radius(*unit_comp);
      const bool filter_enemy = unit_comp->owner_id != m_local_owner_id &&
                                visibility_enabled &&
                                non_local_unit_visibility_filter_enabled();
      const auto visibility_result = m_submission_visibility.evaluate_sphere(
          unit_pos,
          cull_radius,
          filter_enemy ? SubmissionFogMode::VisibleOnly : SubmissionFogMode::Ignore);
      entry.in_frustum = visibility_result.in_frustum;
      entry.fog_visible = visibility_result.fog_visible;

      if (m_camera != nullptr) {
        QVector3D const cam_pos = m_camera->get_position();
        float const dx = unit_pos.x() - cam_pos.x();
        float const dz = unit_pos.z() - cam_pos.z();
        entry.distance_sq = dx * dx + dz * dz;
      }

      if (!entry.in_frustum || !entry.fog_visible) {
        continue;
      }
      ++visible_unit_count;

      auto const* presentation =
          entity->get_component<Engine::Core::CreaturePresentationComponent>();
      entry.combat_active = is_unit_combat_active(presentation);
      entry.has_attack = presentation != nullptr && presentation->snapshot_valid &&
                         presentation->is_in_melee_lock;

      entry.has_guard_mode = presentation != nullptr && presentation->snapshot_valid &&
                             presentation->guard_mode_indicator;
      entry.has_hold_mode = presentation != nullptr && presentation->snapshot_valid &&
                            presentation->hold_mode_indicator;
      entry.has_patrol = presentation != nullptr && presentation->snapshot_valid &&
                         presentation->patrol_mode_indicator;

      unit_entries.push_back(std::move(entry));
    }
  }

  std::stable_sort(unit_entries.begin(),
                   unit_entries.end(),
                   [](const UnitRenderEntry& lhs, const UnitRenderEntry& rhs) {
                     if (lhs.distance_sq != rhs.distance_sq) {
                       return lhs.distance_sq < rhs.distance_sq;
                     }
                     return lhs.entity_id < rhs.entity_id;
                   });

  auto collect_non_unit_entry = [&](std::uint32_t entity_id,
                                    std::vector<RenderEntry>& dest,
                                    float cull_radius) {
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

    float distance_sq = 0.0F;
    const QVector3D position(
        transform->position.x, transform->position.y, transform->position.z);
    const auto fog_mode = visibility_enabled && static_world_visibility_filter_enabled()
                              ? SubmissionFogMode::Revealed
                              : SubmissionFogMode::Ignore;
    if (!m_submission_visibility.accepts_sphere(position, cull_radius, fog_mode)) {
      return;
    }
    if (m_camera != nullptr) {
      const QVector3D camera_position = m_camera->get_position();
      const float dx = position.x() - camera_position.x();
      const float dz = position.z() - camera_position.z();
      distance_sq = dx * dx + dz * dz;
    }

    RenderEntry entry;
    entry.entity = entity;
    entry.renderable = renderable;
    entry.transform = transform;
    entry.unit = nullptr;
    entry.entity_id = entity_id;
    entry.selected = (m_selected_ids.find(entity_id) != m_selected_ids.end());
    entry.hovered = (entity_id == m_hovered_entity_id);
    entry.distance_sq = distance_sq;
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
    collect_non_unit_entry(entity_id, building_entries, 8.0F);
  }

  for (std::uint32_t const entity_id : other_ids) {
    collect_non_unit_entry(entity_id, other_entries, 3.0F);
  }

  m_unit_render_cache.prune(m_frame_counter);
  m_model_matrix_cache.prune(m_frame_counter);
  auto& battle_optimizer = Render::BattleRenderOptimizer::instance();
  battle_optimizer.set_visible_unit_count(visible_unit_count);
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  auto& frame_profile = Render::Profiling::global_profile();
#endif
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

    if (entry.cache == nullptr) {
      continue;
    }
    UnitRenderCache::update_model_matrix(*entry.cache);
    const QMatrix4x4& model_matrix = entry.cache->model_matrix;

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
        ctx.submission_visibility = &m_submission_visibility;
        ctx.submission_fog_mode =
            entry.unit != nullptr && entry.unit->owner_id != m_local_owner_id &&
                    visibility_enabled && non_local_unit_visibility_filter_enabled()
                ? SubmissionFogMode::VisibleOnly
                : SubmissionFogMode::Ignore;
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
            !full_creature_detail && !entry.combat_active && batching_ratio > 0.0F;
        const auto tier = full_creature_detail ? Render::Pipeline::LodTier::Full
                                               : Render::Pipeline::select_lod(lod_in);

        if (!entry.selected && !entry.hovered && !full_creature_detail) {
          if (tier == Render::Pipeline::LodTier::Minimal) {
            ctx.max_rendered_individuals = 4;
          } else if (tier == Render::Pipeline::LodTier::Simplified) {
            ctx.max_rendered_individuals = 8;
          }
        }

        if (full_creature_detail) {
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

#if defined(SOI_ENABLE_RUNTIME_TRACING)
        auto const* animation_debug =
            Render::Profiling::CombatAnimationDiagnostics::instance().find_unit(
                entry.entity_id);
        bool const all_published_soldiers_culled =
            animation_debug != nullptr && !animation_debug->soldiers.empty() &&
            std::all_of(animation_debug->soldiers.begin(),
                        animation_debug->soldiers.end(),
                        [](auto const& soldier) {
                          return soldier.cull_reason !=
                                 Render::Profiling::SoldierCullReason::None;
                        });
#else
        constexpr bool all_published_soldiers_culled = false;
#endif
#if defined(SOI_ENABLE_RUNTIME_TRACING)
        if (entry.unit != nullptr && probe.rigged_body_count() == 0U &&
            unit_should_emit_rigged_body(entry.unit->spawn_type) && !tier_is_minimal &&
            !all_published_soldiers_culled) {
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
#endif

        drawn_by_registry = true;
      }
    }
    if (drawn_by_registry) {

      if (entry.selected || entry.hovered) {
        enqueue_selection_ring(
            entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
      }

      enqueue_mode_indicator(entry.entity_id,
                             entry.transform,
                             entry.unit,
                             entry.has_attack,
                             entry.has_guard_mode,
                             entry.has_hold_mode,
                             entry.has_patrol);
      continue;
    }

    Mesh* mesh_to_draw = resolve_fallback_mesh(entry.renderable);
    QVector3D const color = QVector3D(entry.renderable->color[0],
                                      entry.renderable->color[1],
                                      entry.renderable->color[2]);

    if (entry.selected || entry.hovered) {
      enqueue_selection_ring(
          entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
    }
    enqueue_mode_indicator(entry.entity_id,
                           entry.transform,
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

  // Humanoid preparation happens inside the unit renderer callbacks above, so
  // publish the count only after that pass has populated the per-frame stats.
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  frame_profile.visible_soldiers = get_humanoid_render_stats().soldiers_rendered;
#endif

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
        ctx.distance_sq = entry.distance_sq;
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

  (void)vis;

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

    const bool filter_preview = preview_owner != m_local_owner_id && visibility_enabled;
    const QVector3D preview_position(preview_x, transform->position.y, preview_z);
    if (!m_submission_visibility.accepts_sphere(preview_position,
                                                5.0F,
                                                filter_preview
                                                    ? SubmissionFogMode::VisibleOnly
                                                    : SubmissionFogMode::Ignore)) {
      return;
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
