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
#include <mutex>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "animation/bpat/bpat_registry.h"
#include "battle_render_optimizer.h"
#include "creature/animation_state_components.h"
#include "creature/archetype_registry.h"
#include "creature/pipeline/prepared_submit.h"
#include "creature/pose_intent.h"
#include "creature/quadruped/render_stats.h"
#include "creature/runtime_bake_guard.h"
#include "creature/snapshot_mesh_registry.h"
#include "decoration_gpu.h"
#include "draw_queue.h"
#include "elephant/dimensions.h"
#include "elephant/elephant_renderer_base.h"
#include "entity/building_render_common.h"
#include "entity/carried_load_renderer.h"
#include "entity/registry.h"
#include "entity_appearance.h"
#include "equipment/equipment_registry.h"
#include "equipment/render_archetype_registry.h"
#include "game/accessibility/team_identity.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "game/visuals/team_colors.h"
#include "geom/mode_indicator.h"
#include "gl/backend.h"
#include "gl/buffer.h"
#include "gl/humanoid/animation/animation_inputs.h"
#include "gl/primitives.h"
#include "gl/resources.h"
#include "graphics_settings.h"
#include "horse/dimensions.h"
#include "horse/horse_renderer_base.h"
#include "humanoid/runtime/frame_control.h"
#include "humanoid/runtime/humanoid_renderer.h"
#include "humanoid/runtime/instance_state.h"
#include "humanoid/runtime/runtime_stats.h"
#include "pass/construction_preview_pass.h"
#include "pass/frame_context.h"
#include "pass/primitive_flush_pass.h"
#include "pipeline/lod_selector.h"
#include "pipeline/screen_metrics.h"
#include "primitive_batch.h"
#include "profiling/combat_animation_diagnostics.h"
#include "profiling/frame_profile.h"
#include "render_backend_factory.h"
#include "scene/camera.h"
#include "scene_renderer.h"
#include "selection_ring_layout.h"
#include "software_backend.h"
#include "submitter.h"
#include "template_cache.h"
#include "template_prewarm_catalog.h"
#include "visibility_budget.h"
#include "wildlife/bird_flock_renderer.h"
#include "world_chunk.h"

namespace Render::GL {

namespace {
template <typename ComponentType>
void transfer_render_component(Engine::Core::Entity& previous,
                               Engine::Core::Entity& current) {
  if (auto const* state = previous.get_component<ComponentType>()) {
    if (auto* current_state = current.get_component<ComponentType>()) {
      *current_state = *state;
    } else {
      current.add_component<ComponentType>(*state);
    }
  }
}

void transfer_render_runtime_state(Engine::Core::World& previous,
                                   Engine::Core::World& current) {
  for (Engine::Core::EntityID const id : current.render_unit_ids()) {
    auto* previous_entity = previous.get_entity(id);
    auto* current_entity = current.get_entity(id);
    if (previous_entity == nullptr || current_entity == nullptr) {
      continue;
    }
    transfer_render_component<Render::Creature::HumanoidAnimationStateComponent>(
        *previous_entity, *current_entity);
    transfer_render_component<Render::Creature::HorseAnimationStateComponent>(
        *previous_entity, *current_entity);
    transfer_render_component<Render::Creature::ElephantAnimationStateComponent>(
        *previous_entity, *current_entity);
    transfer_render_component<Render::Creature::HorseAnatomyComponent>(*previous_entity,
                                                                       *current_entity);
    transfer_render_component<Render::Creature::ElephantAnatomyComponent>(
        *previous_entity, *current_entity);
    transfer_render_component<Render::Humanoid::HumanoidInstanceStateComponent>(
        *previous_entity, *current_entity);
  }
}

auto unit_should_emit_rigged_body(Game::Units::SpawnType spawn_type) noexcept -> bool {
  switch (spawn_type) {
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return false;
  default:
    return Game::Units::is_troop_spawn(spawn_type);
  }
}

class RiggedBodyProbeSubmitter final : public ForwardingSubmitter {
public:
  explicit RiggedBodyProbeSubmitter(ISubmitter& inner)
      : ForwardingSubmitter(inner) {}

  [[nodiscard]] auto rigged_body_count() const noexcept -> std::uint32_t {
    return m_rigged_body_count;
  }

  void rigged(const RiggedCreatureCmd& cmd) override {
    if (cmd.mesh != nullptr) {
      ++m_rigged_body_count;
    }
    ForwardingSubmitter::rigged(cmd);
  }

  void rigged(RiggedCreatureCmd&& cmd) override {
    if (cmd.mesh != nullptr) {
      ++m_rigged_body_count;
    }
    ForwardingSubmitter::rigged(std::move(cmd));
  }

private:
  std::uint32_t m_rigged_body_count{0U};
};

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
  const bool mounted = unit.spawn_type == Game::Units::SpawnType::MountedKnight ||
                       unit.spawn_type == Game::Units::SpawnType::HorseArcher ||
                       unit.spawn_type == Game::Units::SpawnType::HorseSpearman;
  const float body_padding = mounted ? 2.75F : 1.75F;
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

} // namespace

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
  Game::Systems::UnitActivity activity{};
  int owner_id{0};
  float indicator_height{0.0F};
  float distance_sq{0.0F};

  float view_distance_sq{0.0F};
  float cull_radius{0.0F};
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

struct UnitSubmitContext {
  Engine::Core::World* world{nullptr};
  ResourceManager* resources{nullptr};
  ISubmitter* batch_submitter{nullptr};

  const Render::BattleRenderOptimizer::FrameSnapshot* optimizer{nullptr};

  Render::BattleRenderOptimizer::FrameStats* optimizer_stats{nullptr};
  Render::Pipeline::ScreenMetrics screen_metrics{};
  float batching_ratio{0.0F};
  float full_shader_max_distance_sq{0.0F};
  std::uint32_t optimizer_frame{0};
  int visible_unit_count{0};
  bool force_batching{false};
  bool never_batch{false};
  bool full_creature_detail{false};
  bool visibility_enabled{false};
};

struct UnitDrawPlan {
  const RenderFunc* fn{nullptr};
  const IParallelPreparer* preparer{nullptr};
  DrawContext draw_ctx{};
  bool use_batching{false};
  bool tier_is_minimal{false};
  int lod_tier{0};
};

namespace {
constexpr float k_selection_marker_thickness = 0.11F;
constexpr float k_selection_marker_min_thickness = 0.05F;
constexpr float k_selected_marker_alpha = 0.7F;
constexpr float k_hovered_marker_alpha = 0.55F;
const QVector3D k_selected_marker_color(0.2F, 0.4F, 1.0F);
const QVector3D k_hovered_marker_color(0.90F, 0.90F, 0.25F);
} // namespace

void Renderer::enqueue_selection_ring(Engine::Core::Entity* entity,
                                      Engine::Core::TransformComponent* transform,
                                      Engine::Core::UnitComponent* unit_comp,
                                      bool selected,
                                      bool hovered) {
  if ((!selected && !hovered) || (transform == nullptr) || m_view.cinematic_mode()) {
    return;
  }

  if (m_view.world_render_mode() == WorldRenderMode::Rpg && entity != nullptr &&
      entity->get_id() == m_view.rpg_camera_focus()) {

    return;
  }

  float ring_size = 0.5F;
  float const ring_offset = 0.05F;
  float ground_offset = 0.0F;
  float scale_y = 1.0F;
  std::vector<SelectionRingPlacement> placements;

  if (unit_comp != nullptr) {
    auto troop_type_opt = Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);
    const auto& config = *world_view().troop_config();

    const auto* profile_ptr =
        troop_type_opt
            ? world_view().find_troop_profile(unit_comp->nation_id, *troop_type_opt)
            : nullptr;
    if (profile_ptr != nullptr) {
      const auto& profile = *profile_ptr;
      auto const formation =
          Game::Systems::FormationCombat::resolve_definition(*unit_comp);
      int const individuals_per_unit = resolved_individuals_per_unit(*unit_comp);

      ring_size =
          Detail::selection_ring_visual_size(config,
                                             unit_comp->spawn_type,
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

      auto const* layout_cache =
          entity != nullptr
              ? entity
                    ->get_component<Render::Humanoid::HumanoidInstanceStateComponent>()
              : nullptr;
      std::span<const Render::Humanoid::SoldierTurnSmoothingState> soldier_anchors;
      if (layout_cache != nullptr) {
        soldier_anchors = layout_cache->turn_states;
      }

      placements = build_selection_ring_layout(
          {.soldiers = soldiers,
           .ring_size = ring_size,
           .position = QVector3D(
               transform->position.x, transform->position.y, transform->position.z),
           .yaw_degrees = transform->rotation.y,
           .soldier_anchors = soldier_anchors,
           .anchor_frame = humanoid_current_frame() + 1U});
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

  const auto ring_pattern =
      Game::Accessibility::TeamIdentity::patterns_enabled() && unit_comp != nullptr
          ? Game::Accessibility::TeamIdentity::pattern_for_slot(unit_comp->owner_id)
          : Game::Accessibility::TeamPattern::Solid;

  const auto& terrain_service = world_view().terrain_or_empty();
  for (const SelectionRingPlacement& placement : placements) {
    QVector3D const grounded_center = terrain_service.resolve_surface_world_position(
        placement.world_x,
        placement.world_z,
        0.0F,
        transform->position.y - ground_offset * scale_y);

    if (!selected && !hovered) {
      continue;
    }

    GroundMarkerCmd marker;
    marker.center = QVector3D(
        grounded_center.x(), grounded_center.y() + ring_offset, grounded_center.z());
    marker.outer_radius = placement.ring_size;
    marker.thickness = std::max(k_selection_marker_min_thickness,
                                placement.ring_size * k_selection_marker_thickness);
    marker.pattern = ring_pattern;
    marker.color = selected ? k_selected_marker_color : k_hovered_marker_color;
    marker.alpha = selected ? k_selected_marker_alpha : k_hovered_marker_alpha;
    marker.focused = hovered;
    ground_marker(marker);
  }
}

void Renderer::refresh_billboard_basis() {
  if (m_camera == nullptr) {
    return;
  }
  const QMatrix4x4 view = m_camera->get_view_matrix();
  m_billboard_right = QVector3D(view(0, 0), view(0, 1), view(0, 2));
  m_billboard_up = QVector3D(view(1, 0), view(1, 1), view(1, 2));
  QVector3D const view_forward =
      QVector3D::crossProduct(m_billboard_right, m_billboard_up).normalized();

  float const tilt_sin = std::sin(Render::Geom::k_indicator_tilt_radians);
  float const tilt_cos = std::cos(Render::Geom::k_indicator_tilt_radians);
  m_billboard_up = (m_billboard_up * tilt_cos - view_forward * tilt_sin).normalized();
  m_billboard_forward =
      QVector3D::crossProduct(m_billboard_right, m_billboard_up).normalized();
}

void Renderer::enqueue_activity_indicator(Engine::Core::EntityID entity_id,
                                          Engine::Core::TransformComponent* transform,
                                          const Game::Systems::UnitActivity& activity,
                                          int owner_id,
                                          float anchor_height,
                                          float distance_sq) {
  if (transform == nullptr || !order_markers_visible_for_owner(owner_id)) {
    return;
  }
  if (!Game::Systems::activity_is_noteworthy(activity) ||
      !Render::Geom::indicator_has_glyph(activity.kind)) {
    return;
  }

  float const distance_fade = Render::Geom::indicator_distance_fade(distance_sq);
  if (distance_fade <= 0.02F) {
    return;
  }

  float const height =
      anchor_height > 0.0F ? anchor_height : Render::Geom::k_indicator_height_base;
  float const scale = Render::Geom::indicator_world_size();
  QVector3D const pos(
      transform->position.x, transform->position.y + height, transform->position.z);

  if (m_camera != nullptr) {
    QVector4D const clip_pos = m_view_proj * QVector4D(pos, 1.0F);
    if (clip_pos.w() > 0.0F) {
      float const inv_w = 1.0F / clip_pos.w();
      float const ndc_x = clip_pos.x() * inv_w;
      float const ndc_y = clip_pos.y() * inv_w;
      float const ndc_z = clip_pos.z() * inv_w;

      constexpr float margin = Render::Geom::k_frustum_cull_margin;
      if (ndc_x < -margin || ndc_x > margin || ndc_y < -margin || ndc_y > margin ||
          ndc_z < -1.0F || ndc_z > 1.0F) {
        return;
      }
    }
  }

  QMatrix4x4 indicator_model;
  indicator_model.setColumn(0, QVector4D(m_billboard_right * scale, 0.0F));
  indicator_model.setColumn(1, QVector4D(m_billboard_up * scale, 0.0F));
  indicator_model.setColumn(2, QVector4D(m_billboard_forward * scale, 0.0F));
  indicator_model.setColumn(3, QVector4D(pos, 1.0F));

  mode_indicator(indicator_model,
                 static_cast<int>(activity.kind),
                 Render::Geom::indicator_color(activity.kind, activity.state),
                 Render::Geom::indicator_state_alpha(activity.state) * distance_fade);
  Render::Profiling::CombatAnimationDiagnostics::instance().record_mode_indicator(
      entity_id);
}

auto Renderer::compute_rpg_lens_gap(Engine::Core::World& world) const
    -> LensGapExclusion {
  constexpr float k_rpg_lens_gap_slack = 0.35F;
  constexpr float k_rpg_lens_gap_focus_radius = 0.45F;

  LensGapExclusion rpg_lens_gap;
  if (m_view.world_render_mode() != WorldRenderMode::Rpg ||
      m_view.rpg_camera_focus() == 0 || m_camera == nullptr) {
    return rpg_lens_gap;
  }

  auto* focus = world.get_entity(m_view.rpg_camera_focus());
  if (focus == nullptr) {
    return rpg_lens_gap;
  }
  auto const* focus_transform =
      focus->get_component<Engine::Core::TransformComponent>();
  if (focus_transform == nullptr) {
    return rpg_lens_gap;
  }

  const QVector3D eye = m_camera->get_position();
  const float to_focus_x = focus_transform->position.x - eye.x();
  const float to_focus_z = focus_transform->position.z - eye.z();
  const float focus_distance = std::hypot(to_focus_x, to_focus_z);
  const float gap_length = focus_distance - k_rpg_lens_gap_slack;
  if (gap_length <= 0.0F || focus_distance <= 0.0001F) {
    return rpg_lens_gap;
  }

  rpg_lens_gap.enabled = true;
  rpg_lens_gap.focus_entity_id = m_view.rpg_camera_focus();
  rpg_lens_gap.eye_x = eye.x();
  rpg_lens_gap.eye_z = eye.z();
  rpg_lens_gap.axis_x = to_focus_x / focus_distance;
  rpg_lens_gap.axis_z = to_focus_z / focus_distance;
  rpg_lens_gap.length = gap_length;
  rpg_lens_gap.focus_radius = k_rpg_lens_gap_focus_radius;
  return rpg_lens_gap;
}

auto Renderer::frame_screen_metrics() const -> Render::Pipeline::ScreenMetrics {
  if (m_camera == nullptr || m_viewport_height <= 0) {
    return {};
  }
  return Render::Pipeline::ScreenMetrics::from_viewport(m_camera->get_fov(),
                                                        m_viewport_height);
}

void Renderer::collect_unit_entries(Engine::Core::World& world,
                                    std::span<const Engine::Core::EntityID> entity_ids,
                                    bool visibility_enabled,
                                    std::vector<UnitRenderEntry>& out,
                                    int& visible_unit_count) {
  for (Engine::Core::EntityID const entity_id : entity_ids) {

    Engine::Core::Entity* entity = world.get_entity(entity_id);
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

      auto& cached = m_unit_render_cache.get_or_create(
          world_view(), entity_id, entity, m_frame_counter);

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
      bool const is_hovered = (entity_id == m_view.hovered_entity_id());
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
      const bool filter_enemy = unit_comp->owner_id != m_view.local_owner_id() &&
                                visibility_enabled &&
                                non_local_unit_visibility_filter_enabled();
      const auto visibility_result = m_submission_visibility.evaluate_sphere(
          unit_pos,
          cull_radius,
          filter_enemy ? SubmissionFogMode::VisibleOnly : SubmissionFogMode::Ignore,
          FogExtent::Anchor);
      entry.in_frustum = visibility_result.in_frustum;
      entry.fog_visible = visibility_result.fog_visible;

      entry.cull_radius = cull_radius;
      if (m_camera != nullptr) {
        QVector3D const cam_pos = m_camera->get_position();
        float const dx = unit_pos.x() - cam_pos.x();
        float const dz = unit_pos.z() - cam_pos.z();
        entry.distance_sq = dx * dx + dz * dz;
        float const dy = unit_pos.y() - cam_pos.y();
        entry.view_distance_sq = entry.distance_sq + dy * dy;
      }

      if (!entry.in_frustum || !entry.fog_visible) {
        Render::Profiling::CombatAnimationDiagnostics::instance().record_unit_cull(
            entity_id,
            entry.in_frustum ? Render::Profiling::SoldierCullReason::Fog
                             : Render::Profiling::SoldierCullReason::Frustum);
        continue;
      }

      ++visible_unit_count;

      auto const* presentation =
          entity->get_component<Engine::Core::CreaturePresentationComponent>();
      entry.combat_active = is_unit_combat_active(presentation);
      if (presentation != nullptr && presentation->snapshot_valid) {
        entry.activity = presentation->activity;
      }
      entry.owner_id = unit_comp->owner_id;
      entry.indicator_height = cached.indicator_height;

      out.push_back(std::move(entry));
    }
  }
}

void Renderer::collect_non_unit_entries(
    Engine::Core::World& world,
    std::span<const Engine::Core::EntityID> entity_ids,
    float cull_radius,
    bool visibility_enabled,
    std::vector<RenderEntry>& out) {
  for (Engine::Core::EntityID const entity_id : entity_ids) {
    Engine::Core::Entity* entity = world.get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }
    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if ((renderable == nullptr) || !renderable->visible) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    float distance_sq = 0.0F;
    const QVector3D position(
        transform->position.x, transform->position.y, transform->position.z);
    const auto fog_mode = visibility_enabled && static_world_visibility_filter_enabled()
                              ? SubmissionFogMode::Revealed
                              : SubmissionFogMode::Ignore;
    if (!m_submission_visibility.accepts_sphere(position, cull_radius, fog_mode)) {
      continue;
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
    entry.hovered = (entity_id == m_view.hovered_entity_id());
    entry.distance_sq = distance_sq;
    if (!renderable->renderer_id.empty()) {
      std::string_view const canonical =
          canonicalize_building_renderer_key(renderable->renderer_id);
      if (m_entity_registry != nullptr) {
        entry.renderer_handle = m_entity_registry->get_handle(canonical);
      }
      entry.renderer_key = std::string(canonical);
    }
    out.push_back(std::move(entry));
  }
}

auto Renderer::plan_unit_entry(UnitRenderEntry& entry,
                               const UnitSubmitContext& ctx) -> UnitDrawPlan {
  UnitDrawPlan plan{};
  if (!entry.in_frustum || !entry.fog_visible || entry.cache == nullptr ||
      m_entity_registry == nullptr) {
    return plan;
  }

  UnitRenderCache::update_model_matrix(*entry.cache, m_last_frame_delta);
  const QMatrix4x4& model_matrix = entry.cache->model_matrix;

  auto const* fn = m_entity_registry->get(entry.renderer_handle);
  if (fn == nullptr && entry.unit != nullptr) {
    const std::string profile_renderer_key =
        Render::resolve_profile_unit_renderer_key(world_view(), *entry.unit);
    if (!profile_renderer_key.empty() && profile_renderer_key != entry.renderer_key) {
      const auto profile_renderer_handle =
          m_entity_registry->get_handle(profile_renderer_key);
      fn = m_entity_registry->get(profile_renderer_handle);
      if (fn != nullptr) {
        entry.renderer_key = profile_renderer_key;
        entry.renderer_handle = profile_renderer_handle;
      }
    }
  }
  if (fn == nullptr) {
    return plan;
  }
  plan.fn = fn;
  plan.preparer = m_entity_registry->get_preparer(entry.renderer_handle);
  if (plan.preparer != nullptr && entry.entity != nullptr) {
    plan.preparer->ensure_prepare_components(*entry.entity);
  }
  {
    DrawContext& draw_ctx = plan.draw_ctx;
    draw_ctx =
        DrawContext{ctx.resources, entry.entity, ctx.world, world_view(), model_matrix};

    draw_ctx.humanoid_runtime = &m_humanoid_runtime;
    draw_ctx.selected = entry.selected;
    draw_ctx.hovered = entry.hovered;
    bool should_update_animation = ctx.full_creature_detail;
    if (!ctx.full_creature_detail) {
      should_update_animation =
          ctx.optimizer->should_update_animation(entry.entity_id,
                                                 entry.distance_sq,
                                                 entry.selected,
                                                 entry.combat_active,
                                                 entry.motion,
                                                 *ctx.optimizer_stats);
    }

    float const animation_time = resolve_animation_time(entry.entity_id,
                                                        should_update_animation,
                                                        m_accumulated_time,
                                                        ctx.optimizer_frame);

    draw_ctx.animation_time = animation_time;
    draw_ctx.distance_sq = entry.distance_sq;
    draw_ctx.renderer_id = entry.renderer_key;
    draw_ctx.renderer_handle = entry.renderer_handle;
    draw_ctx.backend = m_gl_backend;
    draw_ctx.camera = m_camera;
    draw_ctx.order_markers_visible =
        entry.unit != nullptr && order_markers_visible_for_owner(entry.unit->owner_id);
    draw_ctx.submission_visibility = &m_submission_visibility;
    draw_ctx.submission_fog_mode =
        entry.unit != nullptr && entry.unit->owner_id != m_view.local_owner_id() &&
                ctx.visibility_enabled && non_local_unit_visibility_filter_enabled()
            ? SubmissionFogMode::VisibleOnly
            : SubmissionFogMode::Ignore;
    draw_ctx.animation_throttled = !should_update_animation;

    draw_ctx.screen_metrics = ctx.screen_metrics;

    float const projected_radius_px =
        ctx.full_creature_detail
            ? -1.0F
            : ctx.screen_metrics.projected_radius_px_from_distance_sq(
                  entry.view_distance_sq, entry.cull_radius);

    Render::Pipeline::LodInputs lod_in;
    lod_in.distance_sq = entry.distance_sq;
    lod_in.visible_unit_count = ctx.visible_unit_count;
    lod_in.full_detail_max_distance_sq = ctx.full_shader_max_distance_sq;
    lod_in.apparent_size_scale = ctx.screen_metrics.apparent_size_scale();
    lod_in.projected_radius_px = projected_radius_px;
    lod_in.min_projected_radius_px = Render::Pipeline::k_min_unit_projected_radius_px;
    lod_in.selected = entry.selected;
    lod_in.hovered = entry.hovered;
    lod_in.in_frustum = entry.in_frustum;
    lod_in.fog_visible = entry.fog_visible;
    lod_in.force_batching = ctx.force_batching;
    lod_in.never_batch = ctx.never_batch;

    const bool batching_available =
        !ctx.full_creature_detail && ctx.batching_ratio > 0.0F;
    const auto tier = ctx.full_creature_detail ? Render::Pipeline::LodTier::Full
                                               : Render::Pipeline::select_lod(lod_in);

    if (!entry.selected && !entry.hovered && !ctx.full_creature_detail &&
        tier == Render::Pipeline::LodTier::Minimal) {
      draw_ctx.max_rendered_individuals =
          Render::Pipeline::representative_individual_count(projected_radius_px);
    }

    if (ctx.full_creature_detail) {
      draw_ctx.force_humanoid_lod = true;
      draw_ctx.forced_humanoid_lod = HumanoidLOD::Full;
      draw_ctx.force_quadruped_lod = true;
      draw_ctx.forced_quadruped_lod = Render::Creature::CreatureLOD::Full;
    } else if (entry.combat_active) {
      auto const stable_lod = stable_combat_creature_lod(entry.unit, entry.distance_sq);
      draw_ctx.force_humanoid_lod = true;
      draw_ctx.forced_humanoid_lod = stable_lod;
      draw_ctx.force_quadruped_lod = true;
      draw_ctx.forced_quadruped_lod = stable_lod;
    }

    plan.use_batching =
        batching_available && (tier == Render::Pipeline::LodTier::Simplified ||
                               tier == Render::Pipeline::LodTier::Minimal);
    plan.tier_is_minimal = tier == Render::Pipeline::LodTier::Minimal;
    plan.lod_tier = static_cast<int>(tier);
  }
  return plan;
}

void Renderer::submit_unit_entry(
    UnitRenderEntry& entry,
    UnitDrawPlan& plan,
    const UnitSubmitContext& ctx,
    Render::Creature::Pipeline::CreaturePreparationResult* prepared) {
  if (!entry.in_frustum || !entry.fog_visible || entry.cache == nullptr) {
    return;
  }
  const QMatrix4x4& model_matrix = entry.cache->model_matrix;

  bool drawn_by_registry = false;
  bool const tier_is_minimal = plan.tier_is_minimal;
  if (plan.fn != nullptr) {
    {
      RiggedBodyProbeSubmitter probe(
          plan.use_batching ? static_cast<ISubmitter&>(*ctx.batch_submitter)
                            : static_cast<ISubmitter&>(*this));
      if (prepared != nullptr) {
        Render::Creature::Pipeline::submit_preparation(*prepared, probe);
      } else {
        (*plan.fn)(plan.draw_ctx, probe);
      }
      bool const use_batching = plan.use_batching;

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
      if (entry.unit != nullptr && entry.unit->health > 0 &&
          probe.rigged_body_count() == 0U &&
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
                     .arg(plan.lod_tier);
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

    enqueue_activity_indicator(entry.entity_id,
                               entry.transform,
                               entry.activity,
                               entry.owner_id,
                               entry.indicator_height,
                               entry.distance_sq);
    return;
  }

  Mesh* mesh_to_draw = (ctx.resources != nullptr) ? ctx.resources->unit() : nullptr;
  QVector3D const color = Render::entity_color(*entry.entity);

  if (entry.selected || entry.hovered) {
    enqueue_selection_ring(
        entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
  }
  enqueue_activity_indicator(entry.entity_id,
                             entry.transform,
                             entry.activity,
                             entry.owner_id,
                             entry.indicator_height,
                             entry.distance_sq);
  mesh(mesh_to_draw,
       model_matrix,
       color,
       (ctx.resources != nullptr) ? ctx.resources->white() : nullptr,
       1.0F);
}

void Renderer::prepare_unit_plans(std::vector<UnitRenderEntry>& entries,
                                  std::vector<UnitDrawPlan>& plans,
                                  Render::Profiling::FrameProfile& frame_profile) {
  auto const prepare_start = std::chrono::steady_clock::now();
  m_unit_preparations.resize(entries.size());
  for (auto& preparation : m_unit_preparations) {
    preparation.clear();
  }

  auto const& diagnostics = Render::Profiling::CombatAnimationDiagnostics::instance();
  bool const parallel_allowed =
      !diagnostics.enabled() && !diagnostics.logging_enabled();

  auto& parallel_jobs = m_parallel_prepare_jobs;
  parallel_jobs.clear();
  for (std::size_t i = 0; i < entries.size(); ++i) {
    auto& plan = plans[i];
    if (plan.preparer == nullptr) {
      continue;
    }
    auto const handle = entries[i].renderer_handle;
    if (m_prepare_warmed_handles.size() <= handle) {
      m_prepare_warmed_handles.resize(static_cast<std::size_t>(handle) + 1U, 0U);
    }
    if (parallel_allowed && m_prepare_warmed_handles[handle] != 0U) {
      parallel_jobs.push_back(i);
      continue;
    }
    plan.preparer->prepare(plan.draw_ctx, m_unit_preparations[i]);
    m_prepare_warmed_handles[handle] = 1U;
  }

  m_prepare_pool.run(parallel_jobs.size(), [&](std::size_t job) {
    std::size_t const i = parallel_jobs[job];
    plans[i].preparer->prepare(plans[i].draw_ctx, m_unit_preparations[i]);
  });

  frame_profile.humanoid_preparation_us =
      static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - prepare_start)
                                     .count());
}

void Renderer::submit_non_unit_entry(const RenderEntry& entry,
                                     Engine::Core::World* world,
                                     ResourceManager* res) {
  const QMatrix4x4& model_matrix = m_model_matrix_cache.get_or_create(
      entry.entity_id, entry.transform, m_frame_counter);

  bool drawn_by_registry = false;
  if (m_entity_registry &&
      entry.renderer_handle != Render::GL::k_invalid_renderer_handle) {
    auto const* fn = m_entity_registry->get(entry.renderer_handle);
    if (fn != nullptr) {
      DrawContext ctx{resources(), entry.entity, world, world_view(), model_matrix};
      ctx.humanoid_runtime = &m_humanoid_runtime;
      ctx.selected = entry.selected;
      ctx.hovered = entry.hovered;
      ctx.animation_time = m_accumulated_time;
      ctx.distance_sq = entry.distance_sq;
      ctx.renderer_id = entry.renderer_key;
      ctx.renderer_handle = entry.renderer_handle;
      ctx.backend = m_gl_backend;
      ctx.camera = m_camera;
      ctx.order_markers_visible = entry.unit != nullptr &&
                                  order_markers_visible_for_owner(entry.unit->owner_id);
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

  Mesh* mesh_to_draw = (res != nullptr) ? res->unit() : nullptr;
  QVector3D const color = Render::entity_color(*entry.entity);

  if (entry.selected || entry.hovered) {
    enqueue_selection_ring(
        entry.entity, entry.transform, entry.unit, entry.selected, entry.hovered);
  }
  mesh(mesh_to_draw,
       model_matrix,
       color,
       (res != nullptr) ? res->white() : nullptr,
       1.0F);
}
void Renderer::render_world(Engine::Core::World* world) {
  if (m_paused.load()) {
    return;
  }
  if (world == nullptr) {
    return;
  }

  auto& profile = Render::Profiling::global_profile();
  Render::Profiling::PhaseScope const submit_scope(&profile,
                                                   Render::Profiling::Phase::Submit);

  Engine::Core::World* const simulation_world = world;
  if (m_cached_world != nullptr && m_cached_world != simulation_world) {
    m_render_world_snapshot.reset();
  }
  std::shared_ptr<Engine::Core::World> render_snapshot;
  {
    Render::Profiling::PhaseScope const snapshot_scope(
        &profile, Render::Profiling::Phase::Snapshot);
    simulation_world->ensure_render_snapshot();
    render_snapshot = simulation_world->acquire_render_snapshot();
    if (render_snapshot == nullptr) {
      return;
    }
    if (m_render_world_snapshot != nullptr &&
        m_render_world_snapshot.get() != render_snapshot.get()) {
      transfer_render_runtime_state(*m_render_world_snapshot, *render_snapshot);
    }

    m_render_world_snapshot = render_snapshot;
    world = render_snapshot.get();
  }

  std::lock_guard<std::recursive_mutex> const guard(world->get_entity_mutex());

  m_cached_world = simulation_world;

  const bool visibility_enabled = world_view().has_visibility();
  std::span<const Engine::Core::EntityID> const unit_ids = world->render_unit_ids();
  std::span<const Engine::Core::EntityID> const building_ids =
      world->render_building_ids();
  std::span<const Engine::Core::EntityID> const other_ids = world->render_other_ids();

  const auto& gfx_settings = Render::GraphicsSettings::instance();
  const auto& batch_config = gfx_settings.batching_config();
  const bool full_creature_detail =
      m_view.force_full_creature_lod() || !gfx_settings.creature_lod_enabled();

  float camera_height = 0.0F;
  if (m_camera != nullptr) {
    camera_height = m_camera->get_position().y();
  }

  m_submission_visibility.set_lens_gap(compute_rpg_lens_gap(*world));

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

  collect_unit_entries(
      *world, unit_ids, visibility_enabled, unit_entries, visible_unit_count);

  std::stable_sort(unit_entries.begin(),
                   unit_entries.end(),
                   [](const UnitRenderEntry& lhs, const UnitRenderEntry& rhs) {
                     if (lhs.distance_sq != rhs.distance_sq) {
                       return lhs.distance_sq < rhs.distance_sq;
                     }
                     return lhs.entity_id < rhs.entity_id;
                   });

  collect_non_unit_entries(
      *world, building_ids, 8.0F, visibility_enabled, building_entries);
  collect_non_unit_entries(*world, other_ids, 3.0F, visibility_enabled, other_entries);

  m_unit_render_cache.prune(m_frame_counter);
  m_model_matrix_cache.prune(m_frame_counter);
  m_battle_optimizer.set_visible_unit_count(visible_unit_count);
  const auto& optimizer_frame_snapshot = m_battle_optimizer.frame();
  Render::BattleRenderOptimizer::FrameStats optimizer_stats;
  auto& frame_profile = Render::Profiling::global_profile();
  uint32_t const optimizer_frame = optimizer_frame_snapshot.frame;

  float batching_ratio =
      gfx_settings.calculate_batching_ratio(visible_unit_count, camera_height);

  float const batching_boost = optimizer_frame_snapshot.batching_boost();
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

  const UnitSubmitContext submit_ctx{.world = world,
                                     .resources = res,
                                     .batch_submitter = &batch_submitter,
                                     .optimizer = &optimizer_frame_snapshot,
                                     .optimizer_stats = &optimizer_stats,
                                     .screen_metrics = frame_screen_metrics(),
                                     .batching_ratio = batching_ratio,
                                     .full_shader_max_distance_sq =
                                         full_shader_max_distance_sq,
                                     .optimizer_frame = optimizer_frame,
                                     .visible_unit_count = visible_unit_count,
                                     .force_batching = batch_config.force_batching,
                                     .never_batch = batch_config.never_batch,
                                     .full_creature_detail = full_creature_detail,
                                     .visibility_enabled = visibility_enabled};

  static thread_local std::vector<UnitDrawPlan> unit_plans;
  unit_plans.resize(unit_entries.size());
  for (std::size_t i = 0; i < unit_entries.size(); ++i) {
    unit_plans[i] = plan_unit_entry(unit_entries[i], submit_ctx);
  }

  prepare_unit_plans(unit_entries, unit_plans, frame_profile);

  for (std::size_t i = 0; i < unit_entries.size(); ++i) {
    auto& plan = unit_plans[i];
    submit_unit_entry(unit_entries[i],
                      plan,
                      submit_ctx,
                      (plan.preparer != nullptr && i < m_unit_preparations.size())
                          ? &m_unit_preparations[i]
                          : nullptr);
  }

  m_battle_optimizer.commit_frame_stats(optimizer_stats);

  frame_profile.visible_soldiers = get_humanoid_render_stats().soldiers_rendered;

  for (const auto& entry : building_entries) {
    submit_non_unit_entry(entry, world, res);
  }

  for (const auto& entry : other_entries) {
    submit_non_unit_entry(entry, world, res);
  }

  Render::GL::submit_carried_loads(
      world, batch_submitter, &m_submission_visibility, m_camera);

  Render::GL::Wildlife::submit_bird_flocks(
      world_view(), batch_submitter, &m_submission_visibility, m_camera);

  {
    Render::Pass::FrameContext pass_ctx;
    pass_ctx.renderer = this;
    pass_ctx.world = world;
    pass_ctx.queue = m_active_queue;
    pass_ctx.frame_counter = m_frame_counter;
    pass_ctx.view_proj = m_view_proj;
    pass_ctx.primitive_batcher = &batcher;
    pass_ctx.visibility = world_view().visibility();
    pass_ctx.visibility_enabled = visibility_enabled;
    pass_ctx.light_direction = m_light_dir;
    pass_ctx.ambient_strength = m_ambient_strength;

    Render::Pass::PrimitiveFlushPass primitive_flush;
    Render::Pass::ConstructionPreviewPass construction_previews;
    primitive_flush.execute(pass_ctx);
    construction_previews.execute(pass_ctx);
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

    int preview_owner = m_view.local_owner_id();
    if (const auto* preview =
            entity->get_component<Engine::Core::ConstructionPreviewComponent>()) {
      preview_owner = preview->owner_id;
    } else if (const auto* site =
                   entity
                       ->get_component<Engine::Core::WallConstructionSiteComponent>()) {
      preview_owner = site->owner_id;
    }

    const bool filter_preview =
        preview_owner != m_view.local_owner_id() && visibility_enabled;
    const QVector3D preview_position(preview_x, transform->position.y, preview_z);
    if (!m_submission_visibility.accepts_sphere(
            preview_position,
            5.0F,
            filter_preview ? SubmissionFogMode::VisibleOnly : SubmissionFogMode::Ignore,
            FogExtent::Anchor)) {
      return;
    }

    std::string_view const renderer_key =
        canonicalize_building_renderer_key(renderable->renderer_id);
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

    DrawContext ctx{resources(), entity, world, world_view(), model_matrix};
    ctx.humanoid_runtime = &m_humanoid_runtime;
    ctx.selected = false;
    ctx.hovered = false;
    ctx.animation_time = m_accumulated_time;
    ctx.distance_sq = preview_distance_sq;
    ctx.renderer_id = renderer_key;
    ctx.renderer_handle = renderer_handle;
    ctx.backend = m_gl_backend;
    ctx.camera = m_camera;
    ctx.alpha_multiplier = alpha_multiplier;
    ctx.order_markers_visible = order_markers_visible_for_owner(preview_owner);

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
      world->collect_entities_with<Engine::Core::ConstructionPreviewComponent>();
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
      world->collect_entities_with<Engine::Core::WallConstructionSiteComponent>();
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
