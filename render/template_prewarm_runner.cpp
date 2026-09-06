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

#include "animation/bpat/bpat_registry.h"
#include "battle_render_optimizer.h"
#include "creature/archetype_registry.h"
#include "creature/assets/creature_asset_prewarmer.h"
#include "creature/pose_intent.h"
#include "creature/quadruped/render_stats.h"
#include "creature/runtime_bake_guard.h"
#include "creature/snapshot_mesh_registry.h"
#include "decoration_gpu.h"
#include "draw_queue.h"
#include "elephant/dimensions.h"
#include "elephant/elephant_renderer_base.h"
#include "entity/building_render_common.h"
#include "entity/mounted_humanoid_renderer_base.h"
#include "entity/registry.h"
#include "equipment/equipment_registry.h"
#include "equipment/render_archetype_registry.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/combat_rules.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "geom/mode_indicator.h"
#include "gl/backend.h"
#include "gl/buffer.h"
#include "gl/humanoid/animation/animation_inputs.h"
#include "gl/primitives.h"
#include "gl/resources.h"
#include "graphics_settings.h"
#include "horse/dimensions.h"
#include "horse/horse_renderer_base.h"
#include "humanoid/asset/humanoid_asset_prewarmer.h"
#include "humanoid/runtime/frame_control.h"
#include "humanoid/runtime/humanoid_renderer.h"
#include "humanoid/runtime/runtime_stats.h"
#include "pass/construction_preview_pass.h"
#include "pass/frame_context.h"
#include "pass/frame_pass_runner.h"
#include "pass/primitive_flush_pass.h"
#include "pipeline/lod_selector.h"
#include "primitive_batch.h"
#include "profiling/combat_animation_diagnostics.h"
#include "profiling/frame_profile.h"
#include "render/profiling/asset_counters.h"
#include "render_backend_factory.h"
#include "scene/camera.h"
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
  ctx.backend = renderer.backend();
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

void execute_template_prewarm_item(
    Renderer& renderer,
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
    const FacialHairStyle* facial_hair_override = nullptr);

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

} // namespace

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

  const int visible_units = m_battle_optimizer.visible_unit_count();
  if (visible_units >= 300) {
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
      Render::Creature::set_runtime_bake_forbidden(true);
    }
  }
}

void Renderer::prewarm_unit_templates(
    Engine::Core::World* world, TemplatePrewarmProgressCallback progress_callback) {
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::PrewarmInvocations);
  cancel_async_template_prewarm();
  Render::Creature::set_runtime_bake_forbidden(false);
  m_async_prewarm.clear_forbid_runtime_bake();
  if (!m_entity_registry) {
    Render::Profiling::count_asset(
        Render::Profiling::AssetCounter::PrewarmSkippedNoRegistry);
    return;
  }
  const bool full_lod_only =
      m_view.force_full_creature_lod() ||
      !Render::GraphicsSettings::instance().creature_lod_enabled();

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

  if (!report_progress(TemplatePrewarmProgress::Phase::CollectingProfiles, 0, 0)) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, 0, 0);
    return;
  }

  struct PrewarmProfileKey {
    Render::GL::RendererHandle renderer_handle{Render::GL::k_invalid_renderer_handle};
    Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
    Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
    bool operator==(const PrewarmProfileKey& other) const {
      return renderer_handle == other.renderer_handle &&
             spawn_type == other.spawn_type && nation_id == other.nation_id;
    }
  };

  struct PrewarmProfileKeyHash {
    std::size_t operator()(const PrewarmProfileKey& key) const noexcept {
      auto h = static_cast<std::size_t>(key.renderer_handle);
      h ^= static_cast<std::size_t>(key.spawn_type) + 0x9e3779b9 + (h << 6) + (h >> 2);
      h ^= static_cast<std::size_t>(static_cast<std::uint8_t>(key.nation_id)) +
           0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };

  auto is_prewarmable_troop = [](Game::Units::TroopType type) -> bool {
    using Game::Units::TroopType;
    switch (type) {
    case TroopType::Archer:
    case TroopType::Swordsman:
    case TroopType::Spearman:
    case TroopType::RomanLegionOrganizer:
    case TroopType::RomanVeteranConsul:
    case TroopType::RomanFieldCommander:
    case TroopType::CarthageSpearCommander:
    case TroopType::CarthageBowCommander:
    case TroopType::CarthageSwordCommander:
    case TroopType::SkeletonSwordsman:
    case TroopType::SkeletonArcher:
    case TroopType::GravePriest:
    case TroopType::MountedKnight:
    case TroopType::HorseArcher:
    case TroopType::HorseSpearman:
    case TroopType::Healer:
    case TroopType::Civilian:
    case TroopType::Builder:
    case TroopType::Elephant:
      return true;
    case TroopType::Catapult:
    case TroopType::Ballista:
    default:
      return false;
    }
  };

  auto is_prewarmable_spawn =
      [&is_prewarmable_troop](Game::Units::SpawnType spawn_type) -> bool {
    auto const troop_type = Game::Units::spawn_typeToTroopType(spawn_type);
    return troop_type.has_value() && is_prewarmable_troop(*troop_type);
  };

  auto choose_template_budget_by_quality = []() -> std::size_t {
    return Render::GraphicsSettings::instance().prewarm_budget().template_cache_budget;
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

  auto add_profile = [&](const std::string& renderer_id,
                         Game::Units::SpawnType spawn_type,
                         Game::Systems::NationID nation_id,
                         int max_health,
                         bool is_building = false) {
    std::string canonical_renderer_id(renderer_id);
    if (renderer_id.empty() && is_building) {
      canonical_renderer_id =
          building_renderer_key(nation_id, Game::Units::spawn_typeToString(spawn_type));
    } else if (is_building) {
      canonical_renderer_id = resolve_building_renderer_key(
          renderer_id, Game::Units::spawn_typeToString(spawn_type), nation_id);
    } else {
      canonical_renderer_id =
          std::string(canonicalize_building_renderer_key(renderer_id));
    }
    if (!is_prewarmable_spawn(spawn_type) || canonical_renderer_id.empty()) {
      return;
    }
    const auto renderer_handle = m_entity_registry->get_handle(canonical_renderer_id);
    if (renderer_handle == Render::GL::k_invalid_renderer_handle) {
      return;
    }
    PrewarmProfileKey const key{renderer_handle, spawn_type, nation_id};
    if (!profile_seen.insert(key).second) {
      return;
    }
    auto const* fn = m_entity_registry->get(renderer_handle);
    if (fn == nullptr) {
      return;
    }

    PrewarmProfile p;
    p.renderer_id = canonical_renderer_id;
    p.spawn_type = spawn_type;
    p.nation_id = nation_id;
    p.max_health = std::max(1, max_health);
    p.is_elephant = (spawn_type == Game::Units::SpawnType::Elephant);
    p.is_mounted = (spawn_type == Game::Units::SpawnType::MountedKnight ||
                    spawn_type == Game::Units::SpawnType::HorseArcher ||
                    spawn_type == Game::Units::SpawnType::HorseSpearman);
    p.fn = *fn;
    profiles.push_back(std::move(p));
  };

  if (world != nullptr) {
    auto world_units = world->collect_entities_with<Engine::Core::UnitComponent>();
    for (auto* entity : world_units) {
      if (entity == nullptr ||
          entity->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
      if (unit == nullptr || renderable == nullptr || unit->health <= 0) {
        continue;
      }

      if (!is_prewarmable_spawn(unit->spawn_type)) {
        continue;
      }

      add_owner(unit->owner_id);
      active_nation_ids.insert(unit->nation_id);
      add_profile(renderable->renderer_id,
                  unit->spawn_type,
                  unit->nation_id,
                  unit->max_health,
                  entity->get_component<Engine::Core::BuildingComponent>() != nullptr);
    }
  }

  if (world_view().nations() != nullptr) {
    for (const auto& [player_id, nation_id] :
         world_view().nations()->player_nation_assignments()) {
      if (world_view().nations()->get_nation(nation_id) == nullptr) {
        continue;
      }
      add_owner(player_id);
      active_nation_ids.insert(nation_id);
    }
  }

  if (owner_ids.empty() && world_view().owners() != nullptr) {
    const auto& owners = world_view().owners()->get_all_owners();
    for (const auto& owner : owners) {
      add_owner(owner.owner_id);
    }
  }
  if (owner_ids.empty()) {
    add_owner(0);
  }

  {
    static const std::vector<Game::Systems::Nation> k_no_nations;
    const auto& troops = world_view().troop_catalog()->get_all_classes();
    const auto& nations = world_view().nations() != nullptr
                              ? world_view().nations()->get_all_nations()
                              : k_no_nations;
    const bool restrict_to_active_nations = !active_nation_ids.empty();
    const bool has_catalog_entries = !troops.empty();

    auto add_troop_profile = [&](const Game::Systems::Nation& nation,
                                 Game::Units::TroopType type) {
      if (!is_prewarmable_troop(type)) {
        return;
      }

      const auto* profile_ptr = world_view().find_troop_profile(nation.id, type);
      if (profile_ptr == nullptr || profile_ptr->visuals.renderer_id.empty()) {
        return;
      }
      const auto& profile = *profile_ptr;

      add_profile(profile.visuals.renderer_id,
                  Game::Units::spawn_typeFromTroopType(type),
                  nation.id,
                  profile.combat.max_health);
    };

    for (const auto& nation : nations) {
      if (restrict_to_active_nations &&
          (active_nation_ids.find(nation.id) == active_nation_ids.end())) {
        continue;
      }
      if (!restrict_to_active_nations && nation.available_troops.empty()) {
        continue;
      }
      if (has_catalog_entries) {
        for (const auto& entry : troops) {
          add_troop_profile(nation, entry.first);
        }
        continue;
      }

      using Game::Units::TroopType;
      for (auto type : {TroopType::Archer,
                        TroopType::Swordsman,
                        TroopType::Spearman,
                        TroopType::RomanLegionOrganizer,
                        TroopType::RomanVeteranConsul,
                        TroopType::RomanFieldCommander,
                        TroopType::CarthageSpearCommander,
                        TroopType::CarthageBowCommander,
                        TroopType::CarthageSwordCommander,
                        TroopType::SkeletonSwordsman,
                        TroopType::SkeletonArcher,
                        TroopType::GravePriest,
                        TroopType::MountedKnight,
                        TroopType::HorseArcher,
                        TroopType::HorseSpearman,
                        TroopType::Healer,
                        TroopType::Civilian,
                        TroopType::Builder,
                        TroopType::Elephant}) {
        add_troop_profile(nation, type);
      }
    }
  }

  if (profiles.empty()) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  auto profile_priority = [](const PrewarmProfile& profile) -> int {
    if (profile.is_elephant) {
      return 0;
    }
    if (profile.is_mounted) {
      return 1;
    }
    return 2;
  };
  std::stable_sort(profiles.begin(),
                   profiles.end(),
                   [&](const PrewarmProfile& lhs, const PrewarmProfile& rhs) {
                     const int lp = profile_priority(lhs);
                     const int rp = profile_priority(rhs);
                     if (lp != rp) {
                       return lp < rp;
                     }
                     return lhs.renderer_id < rhs.renderer_id;
                   });

  auto preload_profile_archetypes = [&]() {
    if (owner_ids.empty()) {
      return;
    }

    AnimKey idle_key{};
    idle_key.state = Render::Creature::PoseIntent::Idle;
    idle_key.combat_phase = CombatAnimPhase::Idle;
    const int owner_id = owner_ids.front();

    constexpr FacialHairStyle k_facial_hair_styles[] = {
        FacialHairStyle::None,
        FacialHairStyle::Stubble,
        FacialHairStyle::ShortBeard,
        FacialHairStyle::FullBeard,
        FacialHairStyle::LongBeard,
        FacialHairStyle::Goatee,
        FacialHairStyle::Mustache,
        FacialHairStyle::MustacheAndBeard};
    constexpr HumanoidLOD k_preload_lods[] = {HumanoidLOD::Full, HumanoidLOD::Minimal};

    for (std::size_t profile_idx = 0; profile_idx < profiles.size(); ++profile_idx) {
      const PrewarmProfile& profile = profiles[profile_idx];
      for (std::uint8_t variant = 0; variant < k_template_variant_count; ++variant) {
        execute_template_prewarm_item(*this,
                                      profile_idx,
                                      profile.spawn_type,
                                      profile.nation_id,
                                      profile.max_health,
                                      profile.renderer_id,
                                      profile.is_mounted,
                                      profile.is_elephant,
                                      profile.fn,
                                      owner_id,
                                      HumanoidLOD::Full,
                                      variant,
                                      false,
                                      idle_key);
      }
      for (HumanoidLOD const lod : k_preload_lods) {
        if (full_lod_only && lod != HumanoidLOD::Full) {
          continue;
        }
        for (FacialHairStyle const style : k_facial_hair_styles) {
          execute_template_prewarm_item(*this,
                                        profile_idx,
                                        profile.spawn_type,
                                        profile.nation_id,
                                        profile.max_health,
                                        profile.renderer_id,
                                        profile.is_mounted,
                                        profile.is_elephant,
                                        profile.fn,
                                        owner_id,
                                        lod,
                                        0U,
                                        false,
                                        idle_key,
                                        &style);
        }
      }
    }
  };

  preload_profile_archetypes();

  const std::size_t lod_domain_count = full_lod_only ? 1U : 3U;
  const std::size_t domain_count =
      profiles.size() * owner_ids.size() * lod_domain_count;
  if (domain_count == 0) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  const std::size_t target_template_count = choose_template_budget_by_quality();
  auto const anim_catalog = build_template_prewarm_anim_catalog(
      Render::Creature::ArchetypeRegistry::instance());
  auto const anim_selection = select_template_prewarm_anim_budget(
      domain_count, target_template_count, anim_catalog);

  if (anim_selection.selected_core_keys.empty() ||
      anim_selection.variant_values.empty()) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, 0);
    return;
  }

  reset_humanoid_runtime_context();

  std::vector<PrewarmWorkItem> core_work_items =
      build_template_prewarm_work_items(profiles,
                                        owner_ids,
                                        anim_selection.variant_values,
                                        anim_selection.selected_core_keys,
                                        full_lod_only);
  std::vector<PrewarmWorkItem> const extended_work_items =
      build_template_prewarm_work_items(profiles,
                                        owner_ids,
                                        anim_selection.variant_values,
                                        anim_selection.selected_extra_keys,
                                        full_lod_only);

  const std::size_t total_work_count =
      core_work_items.size() + extended_work_items.size();
  if (core_work_items.empty()) {
    Render::Creature::set_runtime_bake_forbidden(true);
    report_progress(TemplatePrewarmProgress::Phase::Completed, 0, total_work_count);
    return;
  }

  if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                       0,
                       core_work_items.size())) {
    report_progress(TemplatePrewarmProgress::Phase::Cancelled, 0, total_work_count);
    return;
  }

  std::vector<std::mutex> profile_mutexes(profiles.size());

  const std::size_t worker_count = 1U;

  std::atomic<std::size_t> next_index{0};
  std::atomic<std::size_t> completed_count{0};
  std::atomic<bool> cancel_requested{false};

  auto worker = [&]() {
    while (true) {
      if (cancel_requested.load(std::memory_order_relaxed)) {
        break;
      }
      const std::size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
      if (idx >= core_work_items.size()) {
        break;
      }

      const PrewarmWorkItem& item = core_work_items[idx];
      const PrewarmProfile& profile = profiles[item.profile_index];

      std::lock_guard<std::mutex> const profile_lock(
          profile_mutexes[item.profile_index]);
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
      completed_count.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> workers;
  workers.reserve(worker_count);
  if (worker_count == 1U) {
    worker();
  } else {
    for (std::size_t i = 0; i < worker_count; ++i) {
      workers.emplace_back(worker);
    }
  }

  constexpr std::size_t k_progress_report_step = 2048;
  std::size_t last_reported = 0;
  while (!cancel_requested.load(std::memory_order_relaxed)) {
    const std::size_t done = std::min(completed_count.load(std::memory_order_relaxed),
                                      core_work_items.size());
    if (done >= core_work_items.size()) {
      break;
    }

    if ((done - last_reported) >= k_progress_report_step) {
      last_reported = done;
      if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                           done,
                           core_work_items.size())) {
        cancel_requested.store(true, std::memory_order_relaxed);
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  for (auto& t : workers) {
    t.join();
  }

  const std::size_t core_done =
      std::min(completed_count.load(std::memory_order_relaxed), core_work_items.size());
  if (cancel_requested.load(std::memory_order_relaxed) ||
      core_done < core_work_items.size()) {
    report_progress(
        TemplatePrewarmProgress::Phase::Cancelled, core_done, total_work_count);
    return;
  }
  if (!report_progress(TemplatePrewarmProgress::Phase::BuildingCoreTemplates,
                       core_done,
                       core_work_items.size())) {
    report_progress(
        TemplatePrewarmProgress::Phase::Cancelled, core_done, total_work_count);
    return;
  }

  if (!extended_work_items.empty()) {
    if (!report_progress(TemplatePrewarmProgress::Phase::QueueingExtendedTemplates,
                         extended_work_items.size(),
                         extended_work_items.size())) {
      report_progress(
          TemplatePrewarmProgress::Phase::Cancelled, core_done, total_work_count);
      return;
    }

    auto async_state = std::make_shared<AsyncTemplatePrewarm::Work>();
    async_state->profiles = profiles;
    async_state->work_items = extended_work_items;
    Render::Profiling::count_asset(Render::Profiling::AssetCounter::PrewarmItemsQueued,
                                   extended_work_items.size());
    m_async_prewarm.start(std::move(async_state), true);
  } else {
    Render::Creature::set_runtime_bake_forbidden(true);
  }

  report_progress(
      TemplatePrewarmProgress::Phase::Completed, core_done, total_work_count);
}

} // namespace Render::GL
