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

auto unit_should_emit_rigged_body(Game::Units::SpawnType spawn_type) noexcept -> bool {
  switch (spawn_type) {
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
  case Game::Units::SpawnType::Ram:
  case Game::Units::SpawnType::SiegeTower:
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

auto stable_combat_creature_lod(const Engine::Core::UnitComponent* unit,
                                float distance_sq) noexcept -> HumanoidLOD {
  const auto& settings = Render::GraphicsSettings::instance();
  float full_distance = settings.humanoid_full_detail_distance();

  if (unit != nullptr) {
    using Game::Units::SpawnType;
    switch (unit->spawn_type) {
    case SpawnType::HorseArcher:
    case SpawnType::HorseSpearman:
    case SpawnType::MountedSwordsman:
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
    draw_ctx = DrawContext{
        ctx.resources,
        entry.entity,
        ctx.world,
        world_view(),
        entry.collapse.active
            ? Render::GL::building_collapse_model(model_matrix, entry.collapse)
        : ctx.world != nullptr ? Render::GL::structure_work_model(
                                     model_matrix, *ctx.world, entry.entity_id)
                               : model_matrix};

    draw_ctx.humanoid_runtime = &m_humanoid_runtime;

    draw_ctx.farm_activity = &m_farm_activity;
    draw_ctx.home_activity = &m_home_activity;
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
            ? non_local_fog_mode(*entry.unit)
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
      ISubmitter& sink = plan.use_batching
                             ? static_cast<ISubmitter&>(*ctx.batch_submitter)
                             : static_cast<ISubmitter&>(*this);
      UnseenSubmitter unseen(sink);
      RiggedBodyProbeSubmitter probe(entry.fog_unseen ? static_cast<ISubmitter&>(unseen)
                                                      : sink);
      if (prepared != nullptr) {
        Render::Creature::Pipeline::submit_preparation(*prepared, probe);
      } else {
        (*plan.fn)(plan.draw_ctx, probe);
      }
      if (entry.unit != nullptr && entry.renderable != nullptr &&
          entry.renderable->structure_foundation_visible &&
          Game::Units::is_building_spawn(entry.unit->spawn_type)) {
        auto& cache = *entry.cache;
        if (!cache.foundation_valid) {
          cache.foundation = Render::GL::resolve_structure_foundation(
              world_view().terrain_or_empty(), entry.unit->spawn_type, model_matrix);
          cache.foundation_valid = true;
        }

        QMatrix4x4 foundation_model = model_matrix;
        if (entry.collapse.active) {
          QMatrix4x4 settle;
          settle.translate(0.0F,
                           -entry.collapse.sink *
                               Render::GL::building_collapse_sink_depth(entry.collapse),
                           0.0F);
          foundation_model = settle * model_matrix;
        }
        Render::GL::submit_structure_foundation(
            cache.foundation,
            foundation_model,
            probe,
            nullptr,
            (ctx.resources != nullptr) ? ctx.resources->white() : nullptr);
      }
      if (entry.collapse.active) {
        Render::GL::submit_building_collapse_rubble(probe, entry.collapse);
      } else if (ctx.world != nullptr && entry.unit != nullptr &&
                 Game::Units::is_building_spawn(entry.unit->spawn_type)) {
        Render::GL::submit_structure_work_dressing(
            probe, *ctx.world, entry.entity_id, plan.draw_ctx.animation_time);
      }
      bool const use_batching = plan.use_batching;

      auto const* animation_debug =
          Render::Profiling::CombatAnimationDiagnostics::instance().find_unit(
              entry.entity_id);
      bool const soldier_visibility_is_known = animation_debug != nullptr;
      bool const all_published_soldiers_culled =
          soldier_visibility_is_known && !animation_debug->soldiers.empty() &&
          std::all_of(animation_debug->soldiers.begin(),
                      animation_debug->soldiers.end(),
                      [](auto const& soldier) {
                        return soldier.cull_reason !=
                               Render::Profiling::SoldierCullReason::None;
                      });
      std::size_t prepared_requests = 0;
      std::size_t prepared_visible_requests = 0;
      if (prepared != nullptr) {
        auto const requests = prepared->bodies.requests();
        prepared_requests = requests.size();
        prepared_visible_requests = static_cast<std::size_t>(
            std::count_if(requests.begin(), requests.end(), [](const auto& request) {
              return request.pass !=
                     Render::Creature::Pipeline::RenderPassIntent::Shadow;
            }));
      }

      const bool prepared_only_casts_shadows = prepared != nullptr &&
                                               prepared_requests > 0 &&
                                               prepared_visible_requests == 0;

      if (entry.unit != nullptr && entry.unit->health > 0 &&
          probe.rigged_body_count() == 0U && soldier_visibility_is_known &&
          unit_should_emit_rigged_body(entry.unit->spawn_type) && !tier_is_minimal &&
          !all_published_soldiers_culled && !prepared_only_casts_shadows) {
        const std::string warning_key =
            std::to_string(entry.entity_id) + ":" + entry.renderer_key;
        if (note_missing_body_warning(warning_key)) {
          qWarning().noquote()
              << QStringLiteral(
                     "Renderer: unit renderer emitted no rigged body; "
                     "entity=%1 renderer='%2' spawn='%3' selected=%4 hovered=%5 "
                     "combat=%6 distance_sq=%7 batching=%8 lod_tier=%9 "
                     "prepared=%10 requests=%11 visible_requests=%12")
                     .arg(entry.entity_id)
                     .arg(QString::fromStdString(entry.renderer_key))
                     .arg(Game::Units::spawn_typeToQString(entry.unit->spawn_type))
                     .arg(static_cast<int>(entry.selected))
                     .arg(static_cast<int>(entry.hovered))
                     .arg(static_cast<int>(entry.combat_active))
                     .arg(entry.distance_sq)
                     .arg(static_cast<int>(use_batching))
                     .arg(plan.lod_tier)
                     .arg(static_cast<int>(prepared != nullptr))
                     .arg(prepared_requests)
                     .arg(prepared_visible_requests);
        }
      }

      drawn_by_registry = true;
    }
  }
  if (drawn_by_registry) {
    if (entry.collapse.active) {
      return;
    }

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

  Engine::Core::World* frozen_world = nullptr;
  for (std::size_t const i : parallel_jobs) {
    if (plans[i].draw_ctx.world != nullptr) {
      frozen_world = plans[i].draw_ctx.world;
      break;
    }
  }
  std::optional<Engine::Core::Registry::StructureFreeze> structure_freeze;
  if (frozen_world != nullptr && !parallel_jobs.empty()) {
    structure_freeze.emplace(frozen_world->registry());
  }
  m_prepare_pool.run(parallel_jobs.size(), [&](std::size_t job) {
    std::size_t const i = parallel_jobs[job];
    plans[i].preparer->prepare(plans[i].draw_ctx, m_unit_preparations[i]);
  });
  structure_freeze.reset();

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

} // namespace Render::GL
