#include <QDebug>
#include <QQuaternion>
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
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "animation/bpat/bpat_registry.h"
#include "game/accessibility/team_identity.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "game/visuals/team_colors.h"
#include "render/battle_render_optimizer.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pose_intent.h"
#include "render/creature/quadruped/render_stats.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/decoration_gpu.h"
#include "render/draw_queue.h"
#include "render/elephant/dimensions.h"
#include "render/elephant/elephant_renderer_base.h"
#include "render/entity/building_collapse.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_worksite.h"
#include "render/entity/carried_load_renderer.h"
#include "render/entity/registry.h"
#include "render/entity/structure_foundation.h"
#include "render/entity/unseen_submitter.h"
#include "render/entity_appearance.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/geom/mode_indicator.h"
#include "render/gl/backend.h"
#include "render/gl/buffer.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/graphics_settings.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_renderer_base.h"
#include "render/humanoid/runtime/frame_control.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/instance_state.h"
#include "render/humanoid/runtime/runtime_stats.h"
#include "render/pass/construction_preview_pass.h"
#include "render/pass/frame_context.h"
#include "render/pass/primitive_flush_pass.h"
#include "render/pipeline/lod_selector.h"
#include "render/pipeline/screen_metrics.h"
#include "render/primitive_batch.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/profiling/frame_profile.h"
#include "render/render_backend_factory.h"
#include "render/scene_renderer.h"
#include "render/scene_walk_internal.h"
#include "render/selection_ring_layout.h"
#include "render/software_backend.h"
#include "render/submitter.h"
#include "render/template_cache.h"
#include "render/template_prewarm_catalog.h"
#include "render/visibility_budget.h"
#include "render/wildlife/bird_flock_renderer.h"
#include "render/world_chunk.h"
#include "scene/camera.h"

namespace Render::GL {

namespace {

constexpr float k_selection_marker_thickness = 0.078F;
constexpr float k_selection_marker_min_thickness = 0.036F;
constexpr float k_selected_marker_alpha = 0.64F;
constexpr float k_hovered_marker_alpha = 0.55F;
const QVector3D k_selected_marker_color(1.0F, 0.80F, 0.42F);
const QVector3D k_hovered_marker_color(0.96F, 0.93F, 0.80F);

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit_comp)
    -> int {
  return Game::Systems::FormationCombat::resolve_definition(unit_comp).total_count;
}

[[nodiscard]] auto preview_structure_spawn_type(const Engine::Core::World& world,
                                                Engine::Core::EntityID id)
    -> std::optional<Game::Units::SpawnType> {
  if (const auto* site = world.try_get<Engine::Core::WallConstructionSiteComponent>(id);
      site != nullptr) {
    return site->product_type;
  }
  if (const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
      unit != nullptr) {
    return unit->spawn_type;
  }
  if (const auto* preview =
          world.try_get<Engine::Core::ConstructionPreviewComponent>(id);
      preview != nullptr && !preview->product_type.empty()) {
    Game::Units::SpawnType type{};
    if (Game::Units::try_parse_spawn_type(QString::fromStdString(preview->product_type),
                                          type) &&
        Game::Units::is_building_spawn(type)) {
      return type;
    }
  }
  return std::nullopt;
}

} // namespace

void Renderer::enqueue_selection_ring(Engine::Core::Entity* entity,
                                      Engine::Core::TransformComponent* transform,
                                      Engine::Core::UnitComponent* unit_comp,
                                      bool selected,
                                      bool hovered) {
  if ((!selected && !hovered) || (transform == nullptr) ||
      !selection_markers_visible()) {
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

      std::span<const Render::Entity::FormationInstance> body_slots;
      if (entity != nullptr && entity->registry() != nullptr) {
        auto const& registry = *entity->registry();
        auto const* creature =
            registry.try_get<Engine::Core::CreaturePresentationComponent>(
                entity->get_id());
        auto const* instance_state =
            registry.try_get<Render::Humanoid::HumanoidInstanceStateComponent>(
                entity->get_id());
        body_slots = selection_ring_body_slots(
            creature != nullptr && creature->is_constructing,
            instance_state != nullptr ? &instance_state->layout : nullptr,
            soldiers.size());
      }

      auto const root = Render::Entity::resolve_formation_root(entity, *transform);
      placements = build_selection_ring_layout({.soldiers = soldiers,
                                                .ring_size = ring_size,
                                                .position = root.position,
                                                .yaw_degrees = root.yaw,
                                                .body_slots = body_slots});
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

  float distance_fade = Render::Geom::indicator_distance_fade(distance_sq);
  if (m_view.world_render_mode() == WorldRenderMode::Rpg) {

    distance_fade *= Render::Geom::indicator_lens_fade(distance_sq);
  }
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
                                        float progress,
                                        bool under_construction) {
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

    if (under_construction) {
      auto const site = Render::GL::building_worksite_for(*world, entity->get_id());
      Render::GL::submit_foundation_curb(*this, site);
      Render::GL::submit_scaffolding(
          *this, site, Render::GL::construction_scaffold_fraction(progress));
      model_matrix = Render::GL::building_standing_model(
          model_matrix,
          preview_position,
          Render::GL::construction_built_fraction(progress));
    }

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

      marker_model.rotate(
          QQuaternion::rotationTo(QVector3D(0.0F, 1.0F, 0.0F),
                                  world_view().terrain_or_empty().sample_ground_normal(
                                      transform->position.x, transform->position.z)));
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
    ctx.order_markers_visible = order_markers_visible_for_owner(preview_owner);

    m_ghost_coverage = alpha_multiplier;
    (*fn)(ctx, *this);
    if (auto const ghost_type = preview_structure_spawn_type(*world, entity->get_id());
        ghost_type.has_value()) {
      auto const foundation = Render::GL::resolve_structure_foundation(
          world_view().terrain_or_empty(), *ghost_type, model_matrix);
      Render::GL::submit_structure_foundation(
          foundation,
          model_matrix,
          *this,
          nullptr,
          (resources() != nullptr) ? resources()->white() : nullptr);
      if (!under_construction && foundation.half_width > 0.0F &&
          !Game::Units::is_wall_network_spawn(*ghost_type)) {
        QVector3D const centre =
            model_matrix.map(QVector3D(foundation.center_x, 0.0F, foundation.center_z));
        QVector3D const corner =
            model_matrix.map(QVector3D(foundation.center_x + foundation.half_width,
                                       0.0F,
                                       foundation.center_z + foundation.half_depth));
        GroundMarkerCmd outline;
        outline.center =
            QVector3D(centre.x(), transform->position.y + 0.06F, centre.z());
        outline.outer_radius =
            std::hypot(corner.x() - centre.x(), corner.z() - centre.z()) + 0.2F;
        outline.thickness = 0.18F;
        outline.color = marker_color;
        outline.alpha = 0.85F;
        ground_marker(outline);
      }
    }
    m_ghost_coverage = 0.0F;

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
    if (preview->site_ghost) {

      const float ghost_alpha =
          0.45F + 0.40F * std::clamp(preview->progress, 0.0F, 1.0F);
      render_preview_like_entity(entity,
                                 ghost_alpha,
                                 QVector3D(0.78F, 0.60F, 0.20F),
                                 0.26F,
                                 preview->progress,
                                 true);
      continue;
    }
    render_preview_like_entity(entity,
                               0.62F,
                               preview->valid ? QVector3D(0.24F, 0.75F, 0.30F)
                                              : QVector3D(0.82F, 0.24F, 0.24F),
                               preview->valid ? 0.28F : 0.36F,
                               0.0F,
                               false);
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
        entity, 0.82F, QVector3D(0.78F, 0.60F, 0.20F), 0.22F, site->progress, true);
  }
}

} // namespace Render::GL
