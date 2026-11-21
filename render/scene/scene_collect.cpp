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
#include "game/systems/nation_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "game/visuals/team_colors.h"
#include "render/backend/backend.h"
#include "render/backend/render_backend_factory.h"
#include "render/backend/software_backend.h"
#include "render/cache/template_cache.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/combat_animation_diagnostics.h"
#include "render/creature/elephant/dimensions.h"
#include "render/creature/elephant/elephant_renderer_base.h"
#include "render/creature/horse/dimensions.h"
#include "render/creature/horse/horse_renderer_base.h"
#include "render/creature/humanoid/animation/animation_inputs.h"
#include "render/creature/humanoid/runtime/frame_control.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/instance_state.h"
#include "render/creature/humanoid/runtime/runtime_stats.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pose_intent.h"
#include "render/creature/quadruped/render_stats.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/creature/wildlife/bird_flock_renderer.h"
#include "render/entity/building_collapse.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_worksite.h"
#include "render/entity/carried_load_renderer.h"
#include "render/entity/registry.h"
#include "render/entity/selection_ring_layout.h"
#include "render/entity/structure_foundation.h"
#include "render/entity/unseen_submitter.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_queue.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/graphics_settings.h"
#include "render/scene/construction_preview_pass.h"
#include "render/frame/pass/frame_context.h"
#include "render/scene/primitive_flush_pass.h"
#include "render/frame/pipeline/lod_selector.h"
#include "render/frame/pipeline/screen_metrics.h"
#include "render/frame/primitive_batch.h"
#include "render/frame/submitter.h"
#include "render/frame/visibility_budget.h"
#include "render/frame/world_chunk.h"
#include "render/geom/mode_indicator.h"
#include "render/gl/buffer.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/profiling/frame_profile.h"
#include "render/scene/battle_render_optimizer.h"
#include "render/scene/scene_renderer.h"
#include "render/scene/scene_walk_internal.h"
#include "render/scene/template_prewarm_catalog.h"
#include "scene/camera.h"

namespace Render::GL {

namespace {

float get_unit_base_cull_radius(Game::Units::SpawnType spawn_type) {
  switch (spawn_type) {
  case Game::Units::SpawnType::MountedSwordsman:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::HorseSpearman:
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
  case Game::Units::SpawnType::Ram:
    return 4.0F;
  case Game::Units::SpawnType::SiegeTower:
    return 7.0F;
  case Game::Units::SpawnType::Elephant:
  case Game::Units::SpawnType::DefenseTower:
    return 5.0F;
  case Game::Units::SpawnType::Barracks:
  case Game::Units::SpawnType::Home:
    return 8.0F;
  case Game::Units::SpawnType::WallSegment:
    return 3.5F;
  case Game::Units::SpawnType::Spearman:
  case Game::Units::SpawnType::Slinger:
  case Game::Units::SpawnType::Velites:
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::Swordsman:
    return 2.5F;
  default:
    return 3.0F;
  }
}

auto is_formation_render_spawn(Game::Units::SpawnType spawn_type) noexcept -> bool {
  using Game::Units::SpawnType;
  switch (spawn_type) {
  case SpawnType::Spearman:
  case SpawnType::Slinger:
  case SpawnType::Velites:
  case SpawnType::Archer:
  case SpawnType::Swordsman:
  case SpawnType::MountedSwordsman:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
    return true;
  default:
    return false;
  }
}

struct CullRadiusKey {
  Game::Units::SpawnType spawn_type;
  Game::Systems::NationID nation_id;
  bool uses_nation_formation_profile;
  int render_individuals_per_unit_override;
  int formation_files_override;
  int squad_strength;

  auto operator==(const CullRadiusKey&) const -> bool = default;
};

struct CullRadiusKeyHash {
  auto operator()(const CullRadiusKey& key) const noexcept -> std::size_t {
    std::size_t hash = static_cast<std::size_t>(key.spawn_type);
    auto mix = [&hash](std::size_t value) {
      hash ^= value + 0x9E3779B97F4A7C15ULL + (hash << 6U) + (hash >> 2U);
    };
    mix(static_cast<std::size_t>(key.nation_id));
    mix(static_cast<std::size_t>(key.uses_nation_formation_profile));
    mix(static_cast<std::size_t>(key.render_individuals_per_unit_override));
    mix(static_cast<std::size_t>(key.formation_files_override));
    mix(static_cast<std::size_t>(key.squad_strength));
    return hash;
  }
};

float compute_unit_cull_radius(const Engine::Core::UnitComponent& unit,
                               float base_radius) {
  const auto definition = Game::Systems::FormationCombat::resolve_definition(unit);
  const int columns = std::clamp(definition.max_per_row, 1, definition.total_count);
  const int rows = (definition.total_count + columns - 1) / columns;
  const float half_width = 0.5F * static_cast<float>(columns - 1) * definition.spacing;
  const float half_depth = 0.5F * static_cast<float>(rows - 1) * definition.spacing;
  const bool mounted = unit.spawn_type == Game::Units::SpawnType::MountedSwordsman ||
                       unit.spawn_type == Game::Units::SpawnType::HorseArcher ||
                       unit.spawn_type == Game::Units::SpawnType::HorseSpearman;
  const float body_padding = mounted ? 2.75F : 1.75F;
  const float formation_radius = std::hypot(half_width, half_depth) + body_padding;
  return std::max(base_radius, formation_radius);
}

float get_unit_cull_radius(const Engine::Core::UnitComponent& unit) {
  const float base_radius = get_unit_base_cull_radius(unit.spawn_type);
  if (!is_formation_render_spawn(unit.spawn_type)) {
    return base_radius;
  }

  thread_local std::unordered_map<CullRadiusKey, float, CullRadiusKeyHash> cache;
  thread_local std::uint64_t cache_epoch = 0;
  const std::uint64_t epoch =
      Game::Systems::FormationCombat::formation_definition_epoch();
  if (epoch != cache_epoch || cache.size() > 4096U) {
    cache.clear();
    cache_epoch = epoch;
  }
  const CullRadiusKey key{
      .spawn_type = unit.spawn_type,
      .nation_id = unit.nation_id,
      .uses_nation_formation_profile = unit.uses_nation_formation_profile,
      .render_individuals_per_unit_override = unit.render_individuals_per_unit_override,
      .formation_files_override = unit.formation_files_override,
      .squad_strength = unit.squad_strength};
  if (auto found = cache.find(key); found != cache.end()) {
    return found->second;
  }
  const float radius = compute_unit_cull_radius(unit, base_radius);
  cache.emplace(key, radius);
  return radius;
}

auto is_unit_combat_active(
    const Engine::Core::CreaturePresentationComponent* presentation) -> bool {
  return presentation != nullptr && presentation->snapshot_valid &&
         presentation->combat_active;
}

[[nodiscard]] auto is_map_landmark(const Engine::Core::UnitComponent& unit) -> bool {

  return unit.spawn_type == Game::Units::SpawnType::Barracks;
}

} // namespace

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
    auto const collapse = Render::GL::resolve_building_collapse(world, entity_id);
    bool const has_death_motion =
        collapse.active ||
        (creature_presentation != nullptr && creature_presentation->snapshot_valid &&
         (creature_presentation->is_dying || creature_presentation->is_dead));
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
      entry.selected = is_selected && !collapse.active;
      entry.hovered = is_hovered && !collapse.active;
      entry.collapse = collapse;
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
      const bool map_landmark = is_map_landmark(*unit_comp);
      const bool filter_enemy =
          !map_landmark && unit_comp->owner_id != m_view.local_owner_id() &&
          visibility_enabled && non_local_unit_visibility_filter_enabled();
      const auto visibility_result = m_submission_visibility.evaluate_sphere(
          unit_pos,
          cull_radius,
          filter_enemy ? non_local_fog_mode(*unit_comp) : SubmissionFogMode::Ignore,
          FogExtent::Anchor);
      entry.in_frustum = visibility_result.in_frustum;
      entry.fog_visible = visibility_result.fog_visible;
      if (filter_enemy && entry.fog_visible) {
        auto const* cover =
            world.try_get<Engine::Core::ForestCoverComponent>(entity_id);
        entry.fog_visible =
            cover == nullptr || !cover->hidden_from(m_view.local_owner_id());
      }
      entry.fog_unseen = map_landmark && visibility_enabled &&
                         static_world_visibility_filter_enabled() &&
                         !m_submission_visibility
                              .evaluate_sphere(unit_pos,
                                               cull_radius,
                                               SubmissionFogMode::Revealed,
                                               FogExtent::Anchor)
                              .fog_visible;

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

} // namespace Render::GL
