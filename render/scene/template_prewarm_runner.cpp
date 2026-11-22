#include <QDebug>
#include <QStringList>
#include <qglobal.h>
#include <qvectornd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "animation/bpat/bpat_registry.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/combat_rules.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "game/visuals/building_asset_key.h"
#include "render/backend/backend.h"
#include "render/backend/render_backend_factory.h"
#include "render/backend/software_backend.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/assets/creature_asset_prewarmer.h"
#include "render/creature/combat_animation_diagnostics.h"
#include "render/creature/elephant/dimensions.h"
#include "render/creature/elephant/elephant_renderer_base.h"
#include "render/creature/horse/dimensions.h"
#include "render/creature/horse/horse_renderer_base.h"
#include "render/creature/humanoid/animation/animation_inputs.h"
#include "render/creature/humanoid/asset/humanoid_asset_prewarmer.h"
#include "render/creature/humanoid/runtime/frame_control.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/runtime_stats.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_asset_init.h"
#include "render/creature/pose_intent.h"
#include "render/creature/quadruped/render_stats.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_render_common.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/civilian_actor_prewarm.h"
#include "render/entity/mounted_humanoid_renderer_base.h"
#include "render/entity/registry.h"
#include "render/entity/selection_ring_layout.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_queue.h"
#include "render/frame/draw_resources.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/pass/frame_context.h"
#include "render/frame/pass/frame_pass_runner.h"
#include "render/frame/pipeline/lod_selector.h"
#include "render/frame/primitive_batch.h"
#include "render/frame/submitter.h"
#include "render/frame/visibility_budget.h"
#include "render/frame/world_chunk.h"
#include "render/geom/mode_indicator.h"
#include "render/geom/primitives.h"
#include "render/gl/buffer.h"
#include "render/profiling/asset_counters.h"
#include "render/profiling/frame_profile.h"
#include "render/scene/battle_render_optimizer.h"
#include "render/scene/construction_preview_pass.h"
#include "render/scene/primitive_flush_pass.h"
#include "render/scene/scene_renderer.h"
#include "render/scene/template_prewarm_catalog.h"
#include "render/scene/template_prewarm_runner_internal.h"
#include "render/scene/template_recorder.h"
#include "scene/camera.h"

namespace Render::GL {

namespace template_prewarm_runner_detail {
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

  auto* renderable = entity.add_component<Engine::Core::RenderableComponent>();
  renderable->renderer_id = renderer_id;
  renderable->visible = true;
}

auto make_template_prewarm_draw_context(Renderer& renderer,
                                        Engine::Core::Entity& entity,
                                        const std::string& renderer_id,
                                        int owner_id,
                                        HumanoidLOD lod,
                                        std::uint8_t variant,
                                        bool allow_template_cache,
                                        bool force_quadruped_lod,
                                        const AnimationInputs* animation_override,
                                        std::uint8_t attack_variant_override,
                                        bool has_attack_variant_override,
                                        const FacialHairStyle* facial_hair_override)
    -> DrawContext {
  DrawContext ctx{
      renderer.resources(), &entity, nullptr, renderer.world_view(), QMatrix4x4()};
  ctx.renderer_id = renderer_id;
  ctx.environment_lighting = renderer.backend() != nullptr
                                 ? &renderer.backend()->environment_lighting()
                                 : nullptr;
  ctx.camera = nullptr;
  ctx.allow_template_cache = allow_template_cache;
  ctx.template_prewarm = true;
  ctx.has_seed_override = true;
  ctx.seed_override = prewarm_seed_for_variant(owner_id, variant);
  ctx.has_variant_override = true;
  ctx.variant_override = variant;
  ctx.force_humanoid_lod = true;
  ctx.forced_humanoid_lod = lod;
  ctx.force_quadruped_lod = force_quadruped_lod;
  if (ctx.force_quadruped_lod) {
    ctx.forced_quadruped_lod = static_cast<Render::Creature::CreatureLOD>(lod);
  }
  ctx.animation_override = animation_override;
  ctx.has_attack_variant_override = has_attack_variant_override;
  ctx.attack_variant_override = attack_variant_override;
  if (facial_hair_override != nullptr) {
    ctx.has_facial_hair_override = true;
    ctx.facial_hair_override = *facial_hair_override;
  }
  return ctx;
}

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
  void ground_marker(const GroundMarkerCmd&) override {}
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
                                   const AnimKey& anim_key,
                                   const FacialHairStyle* facial_hair_override) {
  if (!fn) {
    return;
  }

  Engine::Core::StandaloneEntity scratch(prewarm_entity_id_for_variant(
      profile_index, owner_id, static_cast<std::uint8_t>(lod), variant));
  Engine::Core::Entity& entity = scratch.entity();
  populate_template_prewarm_entity(
      entity, spawn_type, nation_id, owner_id, max_health, renderer_id);

  AnimationInputs const anim = make_animation_inputs(anim_key);
  bool const attack_state = Render::Creature::is_attack_pose_intent(anim_key.state);
  DrawContext const ctx = make_template_prewarm_draw_context(renderer,
                                                             entity,
                                                             renderer_id,
                                                             owner_id,
                                                             lod,
                                                             variant,
                                                             allow_template_cache,
                                                             is_mounted || is_elephant,
                                                             &anim,
                                                             anim_key.attack_variant,
                                                             attack_state,
                                                             facial_hair_override);

  CreatureCacheWarmupSubmitter warmup_submitter(&renderer);
  fn(ctx, warmup_submitter);
}

void prewarm_humanoid_assets_for_profile(Renderer& renderer,
                                         const EntityRendererRegistry* entity_registry,
                                         const PrewarmProfile& profile,
                                         HumanoidLOD lod,
                                         std::uint8_t variant) {
  if (entity_registry == nullptr) {
    return;
  }
  const auto handle = entity_registry->get_handle(profile.renderer_id);
  if (handle == k_invalid_renderer_handle) {
    return;
  }
  const auto* preparer = entity_registry->get_preparer(handle);
  const auto* humanoid = dynamic_cast<const HumanoidRendererBase*>(preparer);
  if (humanoid == nullptr) {
    return;
  }

  Render::Humanoid::HumanoidAssetPrewarmer prewarmer(renderer.rigged_mesh_cache());
  Render::Creature::CreatureAssetPrewarmer creature_prewarmer(
      renderer.rigged_mesh_cache());
  const auto& spec = humanoid->visual_spec();
  prewarmer.prewarm({.archetype = spec.archetype_id,
                     .lod = lod,
                     .variant = static_cast<Render::Creature::VariantId>(variant),
                     .creature_asset = spec.creature_asset_id});

  if (const auto* mounted = dynamic_cast<const MountedHumanoidRendererBase*>(preparer);
      mounted != nullptr) {
    const auto& mounted_spec = mounted->mounted_visual_spec();
    prewarmer.prewarm({.archetype = mounted_spec.rider.archetype_id,
                       .lod = lod,
                       .variant = static_cast<Render::Creature::VariantId>(variant),
                       .creature_asset = mounted_spec.rider.creature_asset_id});
    prewarmer.prewarm({.archetype = mounted_spec.mount.archetype_id,
                       .lod = lod,
                       .variant = static_cast<Render::Creature::VariantId>(variant),
                       .creature_asset = mounted_spec.mount.creature_asset_id});
  }
}

auto prewarm_humanoid_body_variants(Renderer& renderer,
                                    const EntityRendererRegistry* entity_registry,
                                    const std::vector<PrewarmProfile>& profiles,
                                    bool full_lod_only) -> std::size_t {
  using Render::Creature::CreatureLOD;
  std::vector<HumanoidPrewarmTarget> unit_targets;
  if (entity_registry != nullptr) {
    for (const auto& profile : profiles) {
      const auto handle = entity_registry->get_handle(profile.renderer_id);
      if (handle == k_invalid_renderer_handle) {
        continue;
      }
      const auto* humanoid = dynamic_cast<const HumanoidRendererBase*>(
          entity_registry->get_preparer(handle));
      if (humanoid == nullptr) {
        continue;
      }
      for (const auto& target :
           variant_table_prewarm_targets(humanoid->visual_spec())) {
        if (std::find(unit_targets.begin(), unit_targets.end(), target) ==
            unit_targets.end()) {
          unit_targets.push_back(target);
        }
      }
    }
  }

  Render::Humanoid::HumanoidAssetPrewarmer prewarmer(renderer.rigged_mesh_cache());
  std::size_t baked = 0;
  auto bake = [&](const HumanoidPrewarmTarget& target, CreatureLOD lod) {
    if (prewarmer.prewarm({.archetype = target.archetype,
                           .lod = lod,
                           .variant = Render::Creature::k_canonical_variant,
                           .creature_asset = target.asset})) {
      ++baked;
    }
  };

  for (const auto& target : unit_targets) {
    bake(target, CreatureLOD::Full);
    if (!full_lod_only) {
      bake(target, CreatureLOD::Minimal);
    }
  }

  bool const actors_use_minimal = civilian_actor_minimal_lod_allowed();
  for (const auto& target : civilian_actor_prewarm_targets()) {
    bake(target, CreatureLOD::Full);
    if (actors_use_minimal) {
      bake(target, CreatureLOD::Minimal);
    }
  }
  return baked;
}

void prewarm_creature_species(Renderer& renderer, bool full_lod_only) {
  using Render::Creature::CreatureLOD;
  auto const& archetypes = Render::Creature::ArchetypeRegistry::instance();
  auto& handles =
      Render::Creature::Pipeline::CreatureRenderAssetHandleRegistry::instance();
  for (std::size_t index = 0; index < archetypes.size(); ++index) {
    const auto* archetype =
        archetypes.get(static_cast<Render::Creature::ArchetypeId>(index));
    if (archetype == nullptr || archetype->bake_attachment_count != 0U) {
      continue;
    }
    const auto* handle = handles.get(handles.get_or_create(
        Render::Creature::Pipeline::k_invalid_creature_asset, archetype->id));
    if (handle == nullptr || !handle->valid()) {
      continue;
    }
    const Render::Creature::Bpat::BpatBlob* blob = nullptr;
    for (const auto& playback : handle->playback) {
      if (playback.blob != nullptr && playback.frame_count > 0U) {
        blob = playback.blob;
        break;
      }
    }
    if (blob == nullptr) {
      continue;
    }
    for (const CreatureLOD lod : {CreatureLOD::Full, CreatureLOD::Minimal}) {
      if (!full_lod_only || lod == CreatureLOD::Full) {
        (void)Render::Creature::Pipeline::create_creature_render_asset(
            renderer.rigged_mesh_cache(), *handle, lod, *blob, 0U, true);
      }
    }
  }
}

} // namespace template_prewarm_runner_detail

using namespace template_prewarm_runner_detail;

void Renderer::cancel_async_template_prewarm() {
  if (auto work = m_async_prewarm.take()) {
    work->cancel_requested.store(true, std::memory_order_relaxed);
  }
}

void Renderer::run_template_prewarm_item(const PrewarmProfile& profile,
                                         const PrewarmWorkItem& item) {

  prewarm_humanoid_assets_for_profile(
      *this, m_entity_registry.get(), profile, item.lod, item.variant);

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
  const auto state = m_async_prewarm.current();
  if (!state || state->cancel_requested.load(std::memory_order_relaxed)) {
    return;
  }

  const auto& prewarm_budget = Render::GraphicsSettings::instance().prewarm_budget();
  std::size_t max_items = prewarm_budget.items_per_tick;
  std::chrono::microseconds time_budget(prewarm_budget.tick_budget_us);

  constexpr std::chrono::microseconds k_loading_overlay_budget(12000);
  const int visible_units = m_battle_optimizer.visible_unit_count();
  if (m_loading_overlay_active) {
    max_items = std::numeric_limits<std::size_t>::max();
    time_budget = std::max(time_budget, k_loading_overlay_budget);
  } else if (visible_units >= 300) {
    if ((m_battle_optimizer.frame_counter() & 1U) != 0U) {
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
    Render::Profiling::count_asset(
        Render::Profiling::AssetCounter::PrewarmItemsProcessed);
    ++processed;

    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed >= time_budget) {
      break;
    }
  }

  if (state->cancel_requested.load(std::memory_order_relaxed) ||
      (state->next_index.load(std::memory_order_relaxed) >= state->work_items.size())) {
    if (m_async_prewarm.finish(state)) {
      set_forbids_runtime_bake(true);
    }
  }
}

} // namespace Render::GL
