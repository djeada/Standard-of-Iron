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
#include "game/systems/building_collision_registry.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "game/visuals/team_colors.h"
#include "render/backend/backend.h"
#include "render/backend/render_backend_factory.h"
#include "render/backend/software_backend.h"
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
#include "render/equipment/equipment_registry.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_queue.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/pass/frame_context.h"
#include "render/frame/pipeline/lod_selector.h"
#include "render/frame/pipeline/screen_metrics.h"
#include "render/frame/primitive_batch.h"
#include "render/frame/submitter.h"
#include "render/frame/unseen_submitter.h"
#include "render/frame/visibility_budget.h"
#include "render/frame/world_chunk.h"
#include "render/geom/mode_indicator.h"
#include "render/gl/buffer.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/profiling/frame_profile.h"
#include "render/scene/battle_render_optimizer.h"
#include "render/scene/construction_preview_pass.h"
#include "render/scene/primitive_flush_pass.h"
#include "render/scene/scene_renderer.h"
#include "render/scene/scene_walk_internal.h"
#include "render/scene/template_prewarm_catalog.h"
#include "render/scene/template_recorder.h"
#include "scene/camera.h"

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

} // namespace

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

auto Renderer::compute_rpg_lens_gap(Engine::Core::World& world) const
    -> LensGapExclusion {
  constexpr float k_rpg_lens_gap_slack = 0.35F;
  constexpr float k_rpg_lens_gap_focus_radius = 0.45F;

  LensGapExclusion rpg_lens_gap;
  if (!m_view.rpg_lens_gap_applies() || m_camera == nullptr) {
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
  rpg_lens_gap.focus_x = focus_transform->position.x;
  rpg_lens_gap.focus_z = focus_transform->position.z;
  return rpg_lens_gap;
}

auto Renderer::frame_screen_metrics() const -> Render::Pipeline::ScreenMetrics {
  if (m_camera == nullptr || m_viewport_height <= 0) {
    return {};
  }
  return Render::Pipeline::ScreenMetrics::from_viewport(m_camera->get_fov(),
                                                        m_viewport_height);
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

  m_farm_activity.begin_frame(world, m_accumulated_time, simulation_world);

  {
    const auto& lighting = environment_lighting();
    const auto& key = lighting.primary_color;
    const float key_luma = ((key.x() * 0.30F) + (key.y() * 0.59F) + (key.z() * 0.11F)) *
                           lighting.primary_intensity;
    m_home_activity.set_night(std::clamp((0.46F - key_luma) / 0.28F, 0.0F, 1.0F));
  }
  m_home_activity.begin_frame(world, m_accumulated_time, simulation_world);

  m_cached_world = simulation_world;

  Render::Ground::refresh_building_ground<Engine::Core::World,
                                          Engine::Core::BuildingComponent,
                                          Engine::Core::TransformComponent,
                                          Engine::Core::UnitComponent>(
      *world,
      m_building_ground,
      m_building_ground_signature,
      [](const Engine::Core::UnitComponent& unit, float facing_degrees) {
        using Registry = Game::Systems::BuildingCollisionRegistry;
        return Registry::rotate_body_offset(
            Registry::get_building_body(
                std::string(Game::Units::spawn_type_name(unit.spawn_type))),
            facing_degrees);
      });

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

  {
    std::uint64_t full = 0;
    std::uint64_t minimal = 0;
    for (const auto& preparation : m_unit_preparations) {
      for (const auto& request : preparation.bodies.requests()) {
        if (request.pass == Render::Creature::Pipeline::RenderPassIntent::Shadow) {
          continue;
        }
        if (request.lod == Render::Creature::CreatureLOD::Full) {
          ++full;
        } else if (request.lod == Render::Creature::CreatureLOD::Minimal) {
          ++minimal;
        }
      }
    }
    frame_profile.visible_soldiers = full + minimal;
    frame_profile.soldiers_lod_full = full;
    frame_profile.soldiers_lod_minimal = minimal;
  }

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

} // namespace Render::GL
