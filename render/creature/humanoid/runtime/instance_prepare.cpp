#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/clip_manifest.h"
#include "animation/combat_root_motion_manifest.h"
#include "animation/layout_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/micro_variation_manifest.h"
#include "game/core/component.h"
#include "game/core/death_sequence.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/formation/unit_layout_state_system.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/units/spawn_type.h"
#include "render/creature/animation_core_bridge.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/combat_animation_diagnostics.h"
#include "render/creature/humanoid/animation/animation_inputs.h"
#include "render/creature/humanoid/pipeline/humanoid_playback.h"
#include "render/creature/humanoid/runtime/formation_instance_layout.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/humanoid_runtime_types.h"
#include "render/creature/humanoid/runtime/instance_state.h"
#include "render/creature/humanoid/runtime/runtime_context.h"
#include "render/creature/humanoid/runtime/runtime_stats.h"
#include "render/creature/humanoid/runtime/soldier_ground_motion.h"
#include "render/creature/humanoid/runtime/soldier_turn_smoothing.h"
#include "render/creature/humanoid/runtime/unit_layout_spacing.h"
#include "render/creature/pipeline/corpse_sink.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/creature/pipeline/humanoid_transition_continuity.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pose_intent.h"
#include "render/frame/draw_context.h"
#include "render/frame/submission_visibility.h"
#include "render/frame/terrain_contact.h"
#include "render/gl/humanoid/humanoid_constants.h"
#include "render/profiling/frame_profile.h"
#include "scene/camera.h"

namespace Render::Humanoid {

namespace {

constexpr float k_corpse_max_ground_tilt_degrees = 24.0F;

struct CasualtyLaunch {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float pitch{0.0F};
  float roll{0.0F};
};

auto resolve_casualty_launch(
    const Engine::Core::SoldierCasualtyAnimationComponent::Entry& entry,
    float velocity_x,
    float velocity_z) -> CasualtyLaunch {
  if (!entry.launched) {
    return {};
  }

  constexpr float k_half_gravity = 4.9F;
  constexpr float k_max_launch_pitch_degrees = 70.0F;
  constexpr float k_max_launch_roll_degrees = 24.0F;
  constexpr float k_pi = 3.14159265F;

  float const landing_time = std::max(0.0F, entry.launch_velocity_y / k_half_gravity);
  float const flight_time =
      std::min(Engine::Core::death_sequence_elapsed(entry), landing_time);

  float const arc =
      landing_time > 0.0F ? std::sin(k_pi * flight_time / landing_time) : 0.0F;
  float const pitch_amplitude =
      std::clamp(entry.launch_pitch_speed * landing_time * 0.25F,
                 -k_max_launch_pitch_degrees,
                 k_max_launch_pitch_degrees);
  float const roll_amplitude =
      std::clamp(entry.launch_roll_speed * landing_time * 0.25F,
                 -k_max_launch_roll_degrees,
                 k_max_launch_roll_degrees);

  return {velocity_x * flight_time,
          std::max(0.0F,
                   entry.launch_velocity_y * flight_time -
                       k_half_gravity * flight_time * flight_time),
          velocity_z * flight_time,
          pitch_amplitude * arc,
          roll_amplitude * arc};
}

constexpr float k_soldier_cull_enter_band = 1.5F;
constexpr float k_soldier_cull_keep_band = 5.0F;
constexpr std::uint32_t k_soldier_visibility_memory_frames = 12U;
constexpr std::uint32_t k_selection_refresh_period = 8U;

constexpr float k_wade_pose_start_depth = 0.12F;
constexpr float k_wade_pose_full_span = 0.20F;

auto humanoid_selection_is_steady(
    const Render::GL::HumanoidAnimationContext& anim_ctx) noexcept -> bool {
  auto const& in = anim_ctx.inputs;
  if (in.is_attacking || in.is_in_melee_lock || in.is_casting || in.is_mounted ||
      in.has_showcase_clip || in.has_authored_action_clip || in.is_in_hold_mode ||
      in.is_exiting_hold || in.is_guarding || in.is_exiting_guard ||
      in.is_hit_reacting || in.is_healing || in.is_routing || in.is_constructing ||
      in.is_carrying_load || in.is_wading || in.is_dying || in.is_dead ||
      in.is_defensive_layout_locked ||
      in.shield_formation_pose != Render::GL::ShieldFormationPose::None ||
      in.combat_visual.authoritative ||
      anim_ctx.ambient_idle_type != Render::GL::AmbientIdleType::None) {
    return false;
  }

  auto const resolved = Render::Creature::is_running_animation(in.movement_state)
                            ? Render::Creature::AnimationStateId::Run
                            : (Render::Creature::is_moving_animation(in.movement_state)
                                   ? Render::Creature::AnimationStateId::Walk
                                   : Render::Creature::AnimationStateId::Idle);
  return !Animation::resolve_locomotion_crossfade(
              {
                  .resolved = resolved,
                  .locomotion_presence = anim_ctx.gait.locomotion_presence,
                  .run_presence = anim_ctx.gait.run_presence,
              })
              .active;
}

namespace RCP = Render::Creature::Pipeline;

struct HumanoidUnitSnapshot {
  const HumanoidRendererBase* owner{nullptr};
  const DrawContext* ctx{nullptr};

  Engine::Core::UnitComponent* unit_comp{nullptr};
  Engine::Core::TransformComponent* transform_comp{nullptr};
  const Engine::Core::FormationPresentationComponent* formation_presentation{nullptr};
  const Engine::Core::CreaturePresentationComponent* creature_presentation{nullptr};
  const Engine::Core::FormationHitPresentationComponent* formation_hit{nullptr};
  Engine::Core::SoldierCasualtyAnimationComponent* casualties_comp{nullptr};

  FormationParams formation{};
  std::uint32_t seed{0U};
  int rows{1};
  int cols{1};
  int total_layout_count{0};
  int visible_count{0};
  std::size_t active_casualty_count{0U};
  bool is_mounted_spawn{false};
  bool has_entity_death{false};
  bool has_shared_formation_layout{false};

  std::vector<int>* live_slot_indices{nullptr};

  HumanoidVariant variant{};
  Render::Creature::Pipeline::UnitVisualSpec visual_spec{};
  float height_scale{1.0F};
  bool needs_height_scaling{false};
  float entity_ground_offset{0.0F};

  Game::Formation::UnitLayoutId unit_layout{Game::Formation::k_invalid_layout};
  float formed_ratio{1.0F};
  Game::Formation::UnitLayoutId blend_from_layout{Game::Formation::k_invalid_layout};
  float layout_blend_ratio{1.0F};
};

struct HumanoidFormationRuntime {
  HumanoidInstanceStateComponent* layout_cache_comp{nullptr};
  std::vector<Render::Entity::FormationInstance>* soldier_layout_storage{nullptr};
  RCP::CreatureLodConfig lod_config{};
  bool allow_animation_persistence{false};
  bool use_per_soldier_locomotion_state{false};
  bool preserve_soldier_state_prefix{false};
};

struct HumanoidUnitAnimationRuntime {
  Render::Creature::HumanoidAnimationStateComponent* humanoid_anim_state{nullptr};
  Render::GL::VisualMovementState visual_movement{};
  float local_enemy_pressure{0.0F};
  Animation::HumanoidCommanderJumpPose commander_jump{};
  Animation::HumanoidCommanderFlagRallyPose rally_pose{};
  float unit_health_ratio{1.0F};
  std::uint32_t unit_attack_target_id{0U};
  bool unit_attack_target_alive{false};
  bool record_animation_diagnostics{false};

  Render::Profiling::CombatAnimationDiagnostics* animation_diagnostics{nullptr};
  bool formation_fight_active{false};

  DrawContext inst_ctx{};
  QMatrix4x4 unit_base{};
  QVector3D root_position{};
  float root_yaw{0.0F};
  Animation::HumanoidLocomotionActionOverrideSample locomotion_override{};

  float turn_smoothing_dt{0.0F};
  bool turn_smoothing_active{false};
  float turn_smoothing_cap{2.5F};
  float ground_march_speed{2.0F};
  bool turn_smoothing_travel_yaw{true};
  bool turn_smoothing_stagger{true};
  bool turn_smoothing_pivot_wheel{true};

  bool unit_is_archer{false};
  std::uint32_t ctx_entity_id{0U};
  QVector3D unit_origin{};
  bool unit_fog_visible{true};

  Render::GL::HumanoidRenderStats* stats{nullptr};
};

auto snapshot_humanoid_unit(const HumanoidRendererBase& owner,
                            const DrawContext& ctx,
                            const AnimationInputs& anim) -> HumanoidUnitSnapshot {
  using namespace Render::GL;

  HumanoidUnitSnapshot snapshot;
  snapshot.owner = &owner;
  snapshot.ctx = &ctx;

  thread_local std::vector<int> live_slot_indices;
  live_slot_indices.clear();
  snapshot.live_slot_indices = &live_slot_indices;

  if (ctx.entity != nullptr && ctx.world == nullptr) {

    Engine::Core::publish_creature_presentation(ctx.entity, nullptr);
  }

  FormationParams const formation = HumanoidRendererBase::resolve_formation(owner, ctx);

  Engine::Core::UnitComponent* unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  }
  auto const* formation_presentation =
      ctx.entity != nullptr
          ? ctx.entity->get_component<Engine::Core::FormationPresentationComponent>()
          : nullptr;
  auto const* creature_presentation =
      ctx.entity != nullptr
          ? ctx.entity->get_component<Engine::Core::CreaturePresentationComponent>()
          : nullptr;
  auto const* formation_hit =
      ctx.entity != nullptr
          ? ctx.entity->get_component<Engine::Core::FormationHitPresentationComponent>()
          : nullptr;

  Engine::Core::TransformComponent* transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    transform_comp = ctx.entity->get_component<Engine::Core::TransformComponent>();
  }
  float entity_ground_offset =
      owner.resolve_entity_ground_offset(ctx, unit_comp, transform_comp);

  uint32_t seed = 0U;
  if (ctx.has_seed_override) {
    seed = ctx.seed_override;
  } else if (formation_presentation != nullptr) {
    seed = formation_presentation->formation_seed;
  } else if (ctx.entity != nullptr) {
    seed = ctx.entity->get_id() * 0x9E3779B9U;
    if (unit_comp != nullptr) {
      seed ^= static_cast<std::uint32_t>(unit_comp->owner_id) * 0x85EBCA6BU;
    }
  }

  int cols =
      std::max(1, std::min(formation.max_per_row, formation.individuals_per_unit));
  const int rows = std::max(1, (formation.individuals_per_unit + cols - 1) / cols);
  int effective_rows = rows;
  if (ctx.force_single_soldier) {
    cols = 1;
    effective_rows = 1;
  }

  bool is_mounted_spawn = owner.uses_mounted_pipeline();
  if (unit_comp != nullptr) {
    using Game::Units::SpawnType;
    auto const st = unit_comp->spawn_type;
    is_mounted_spawn = is_mounted_spawn || st == SpawnType::MountedSwordsman ||
                       st == SpawnType::HorseArcher || st == SpawnType::HorseSpearman;
  }

  int const total_layout_count =
      std::min(formation.individuals_per_unit, effective_rows * cols);
  bool const has_entity_death = anim.is_dying || anim.is_dead;
  auto* casualties_comp =
      (ctx.entity != nullptr)
          ? ctx.entity->get_component<Engine::Core::SoldierCasualtyAnimationComponent>()
          : nullptr;
  bool const has_shared_formation_layout =
      !ctx.force_single_soldier && formation_presentation != nullptr &&
      formation_presentation->soldiers.size() ==
          static_cast<std::size_t>(total_layout_count);
  if (has_shared_formation_layout) {
    live_slot_indices.reserve(formation_presentation->soldiers.size());
    for (auto const& soldier : formation_presentation->soldiers) {
      if (soldier.alive && soldier.slot_index < total_layout_count) {
        live_slot_indices.push_back(static_cast<int>(soldier.slot_index));
      }
    }
  } else if (ctx.entity != nullptr) {
    for (auto const slot : Game::Systems::FormationCombat::living_slot_indices(
             *ctx.entity, total_layout_count)) {
      live_slot_indices.push_back(static_cast<int>(slot));
    }
  } else {
    for (int index = 0; index < total_layout_count; ++index) {
      live_slot_indices.push_back(index);
    }
  }
  bool const casualty_sequence_owns_last_body =
      casualties_comp != nullptr && !casualties_comp->entries.empty();
  if (has_entity_death && live_slot_indices.empty() &&
      !casualty_sequence_owns_last_body) {
    live_slot_indices.push_back(std::max(0, total_layout_count - 1));
  }
  if (ctx.max_rendered_individuals > 0 &&
      live_slot_indices.size() >
          static_cast<std::size_t>(ctx.max_rendered_individuals)) {
    std::vector<int> representative_slots;
    auto const representative_count =
        static_cast<std::size_t>(std::max(1, ctx.max_rendered_individuals));
    representative_slots.reserve(representative_count);
    if (representative_count == 1U) {
      representative_slots.push_back(live_slot_indices[live_slot_indices.size() / 2U]);
    } else {
      for (std::size_t sample = 0; sample < representative_count; ++sample) {
        std::size_t const source =
            (sample * (live_slot_indices.size() - 1U)) / (representative_count - 1U);
        representative_slots.push_back(live_slot_indices[source]);
      }
    }
    live_slot_indices = std::move(representative_slots);
  }
  int const visible_count = static_cast<int>(live_slot_indices.size());
  std::size_t active_casualty_count = 0U;
  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    active_casualty_count = static_cast<std::size_t>(
        std::count_if(casualties_comp->entries.begin(),
                      casualties_comp->entries.end(),
                      [total_layout_count](const auto& entry) {
                        return entry.slot_index < total_layout_count &&
                               std::find(live_slot_indices.begin(),
                                         live_slot_indices.end(),
                                         static_cast<int>(entry.slot_index)) ==
                                   live_slot_indices.end();
                      }));
  }

  HumanoidVariant variant;
  owner.get_variant(ctx, seed, variant);
  if (ctx.has_facial_hair_override) {
    variant.facial_hair.style = ctx.facial_hair_override;
    if (variant.facial_hair.style != FacialHairStyle::None) {
      variant.facial_hair.coverage = std::max(variant.facial_hair.coverage, 1.0F);
    }
  }
  seed_missing_humanoid_wear(variant, seed);
  const auto visual_spec = Render::Creature::Pipeline::resolve_unit_visual_spec(
      owner.visual_spec(), variant);

  const QVector3D prop_scale = owner.visual_spec().scaling.as_vector();
  const float height_scale = prop_scale.y();
  const bool needs_height_scaling = std::abs(height_scale - 1.0F) > 0.001F;

  const QMatrix4x4 k_identity_matrix;

  Game::Formation::UnitLayoutId unit_layout = ctx.world_view.unit_layouts()->resolve(
      Game::Formation::k_neutral_doctrine, "close_order_infantry");
  float formed_ratio = 1.0F;
  Game::Formation::UnitLayoutId blend_from_layout = Game::Formation::k_invalid_layout;
  float layout_blend_ratio = 1.0F;
  if (unit_comp != nullptr) {
    auto const definition =
        Game::Systems::FormationCombat::resolve_definition(*unit_comp);
    unit_layout = definition.layout;

    bool const is_constructing =
        unit_comp->spawn_type == Game::Units::SpawnType::Builder &&
        anim.is_constructing;
    if (is_constructing) {
      unit_layout =
          ctx.world_view.unit_layouts()->resolve(definition.doctrine, "work_party");
    }
  }
  if (ctx.entity != nullptr) {
    if (const auto* layout_state =
            ctx.entity->get_component<Engine::Core::UnitLayoutStateComponent>()) {
      auto const blend =
          Game::Formation::UnitLayoutStateSystem::layout_blend(*ctx.entity);
      formed_ratio = blend.formed_ratio;
      if (layout_state->layout_id != Game::Formation::k_invalid_layout) {
        unit_layout = layout_state->layout_id;
      }
      blend_from_layout = blend.blend_from;
      layout_blend_ratio = blend.blend_ratio;
    }
  }

  snapshot.unit_comp = unit_comp;
  snapshot.transform_comp = transform_comp;
  snapshot.formation_presentation = formation_presentation;
  snapshot.creature_presentation = creature_presentation;
  snapshot.formation_hit = formation_hit;
  snapshot.casualties_comp = casualties_comp;
  snapshot.formation = formation;
  snapshot.seed = seed;
  snapshot.rows = rows;
  snapshot.cols = cols;
  snapshot.total_layout_count = total_layout_count;
  snapshot.visible_count = visible_count;
  snapshot.active_casualty_count = active_casualty_count;
  snapshot.is_mounted_spawn = is_mounted_spawn;
  snapshot.has_entity_death = has_entity_death;
  snapshot.has_shared_formation_layout = has_shared_formation_layout;
  snapshot.variant = variant;
  snapshot.visual_spec = visual_spec;
  snapshot.height_scale = height_scale;
  snapshot.needs_height_scaling = needs_height_scaling;
  snapshot.entity_ground_offset = entity_ground_offset;
  snapshot.unit_layout = unit_layout;
  snapshot.formed_ratio = formed_ratio;
  snapshot.blend_from_layout = blend_from_layout;
  snapshot.layout_blend_ratio = layout_blend_ratio;
  return snapshot;
}

void ease_soldier_offsets(HumanoidInstanceStateComponent& cache,
                          std::vector<Render::Entity::FormationInstance>& instances,
                          float time,
                          bool about_faced,
                          bool enabled) {
  constexpr float k_reshape_speed = 2.5F;
  constexpr float k_reshape_max_dt = 0.1F;
  constexpr float k_reshape_snap_distance = 12.0F;
  constexpr float k_reshape_jump_distance = 0.25F;
  float const dt = cache.offset_time_valid
                       ? std::clamp(time - cache.offset_time, 0.0F, k_reshape_max_dt)
                       : 0.0F;
  bool const reset = !enabled || !cache.offset_time_valid ||
                     cache.displayed_offsets.size() != instances.size() ||
                     cache.offset_about_faced != about_faced;
  cache.offset_time = time;
  cache.offset_time_valid = enabled;
  cache.offset_about_faced = about_faced;
  if (reset) {
    cache.displayed_offsets.resize(instances.size());
    for (std::size_t index = 0; index < instances.size(); ++index) {
      cache.displayed_offsets[index] = {instances[index].offset_x,
                                        instances[index].offset_z};
    }
    return;
  }
  float const max_step = k_reshape_speed * dt;
  for (std::size_t index = 0; index < instances.size(); ++index) {
    auto& shown = cache.displayed_offsets[index];
    auto& instance = instances[index];
    float const dx = instance.offset_x - shown.x;
    float const dz = instance.offset_z - shown.z;
    float const distance = std::sqrt(dx * dx + dz * dz);
    if (distance <= std::max(max_step, k_reshape_jump_distance) ||
        distance >= k_reshape_snap_distance) {
      shown = {instance.offset_x, instance.offset_z};
      continue;
    }
    float const scale = max_step / distance;
    shown.x += dx * scale;
    shown.z += dz * scale;
    instance.offset_x = shown.x;
    instance.offset_z = shown.z;
  }
}

auto prepare_formation_runtime(const HumanoidUnitSnapshot& s,
                               const AnimationInputs& anim,
                               std::uint32_t frame_index) -> HumanoidFormationRuntime {
  using namespace Render::GL;

  const DrawContext& ctx = *s.ctx;
  auto& profile = Render::Profiling::global_profile();
  const FormationParams& formation = s.formation;
  const std::uint32_t seed = s.seed;
  const int rows = s.rows;
  const int cols = s.cols;
  const int total_layout_count = s.total_layout_count;
  const auto* formation_presentation = s.formation_presentation;
  const bool has_shared_formation_layout = s.has_shared_formation_layout;
  const auto unit_layout = s.unit_layout;
  const float formed_ratio = s.formed_ratio;
  const auto blend_from_layout = s.blend_from_layout;
  const float layout_blend_ratio = s.layout_blend_ratio;

  thread_local std::vector<Render::Entity::FormationInstance> transient_layouts;
  transient_layouts.clear();

  const auto lod_config = RCP::humanoid_lod_config_from_settings();
  HumanoidInstanceStateComponent* layout_cache_comp =
      (ctx.entity != nullptr)
          ? Engine::Core::get_or_add_component<HumanoidInstanceStateComponent>(
                ctx.entity)
          : nullptr;
  bool const allow_animation_persistence = should_persist_animation_state(ctx);

  Render::Entity::FormationLayoutRequest layout_request{};
  layout_request.formation = formation;
  layout_request.unit_layout = unit_layout;
  layout_request.rows = rows;
  layout_request.cols = cols;
  layout_request.total_count = total_layout_count;
  layout_request.seed = seed;
  layout_request.animation_time = anim.time;
  layout_request.melee_attack = anim.is_attacking && anim.is_melee;
  layout_request.force_single_soldier = ctx.force_single_soldier;
  layout_request.formed_ratio = formed_ratio;
  layout_request.blend_from = blend_from_layout;
  layout_request.blend_ratio = layout_blend_ratio;
  layout_request.frame_index = frame_index;
  layout_request.layout_version = k_humanoid_layout_cache_version;

  bool const layout_transition_active =
      blend_from_layout != Game::Formation::k_invalid_layout &&
      blend_from_layout != unit_layout && layout_blend_ratio < 0.999F;
  layout_request.max_cache_age =
      (layout_request.melee_attack || layout_transition_active)
          ? 0U
          : ::Render::GL::k_layout_cache_max_age;
  layout_request.soldier_offsets = ctx.world_view.soldier_offsets();

  Render::Entity::FormationLayoutResult layout_result;
  {
    Render::Profiling::AccumulatorScope const layout_scope(
        &profile.soldier_layout_generation_us);
    layout_result = Render::Entity::resolve_formation_instances(
        layout_cache_comp != nullptr ? &layout_cache_comp->layout : nullptr,
        transient_layouts,
        layout_request);
  }
  auto* soldier_layout_storage = layout_result.instances;
  bool const preserve_soldier_state_prefix = layout_result.preserve_state_prefix;
  bool const preserve_soldier_locomotion = layout_result.preserve_locomotion_state;

  auto& soldier_layouts = *soldier_layout_storage;

  bool about_faced = false;
  if (!anim.is_constructing) {
    if (ctx.world != nullptr && ctx.entity != nullptr) {
      auto const* traversal =
          ctx.world->try_get<Engine::Core::UnitTraversalLayoutStateComponent>(
              ctx.entity->get_id());
      about_faced = traversal != nullptr && traversal->about_faced;
    }
    Render::Entity::apply_authoritative_formation_slots(
        std::span<Render::Entity::FormationInstance>(soldier_layouts.data(),
                                                     soldier_layouts.size()),
        has_shared_formation_layout ? formation_presentation : nullptr,
        ctx.entity,
        about_faced,
        ctx.force_single_soldier);
  }

  if (layout_cache_comp != nullptr) {
    ease_soldier_offsets(*layout_cache_comp,
                         soldier_layouts,
                         anim.time,
                         about_faced,
                         allow_animation_persistence && !anim.is_constructing &&
                             !ctx.force_single_soldier && total_layout_count > 1);
  }

  bool const use_per_soldier_locomotion_state = total_layout_count > 1;
  if (layout_cache_comp != nullptr && use_per_soldier_locomotion_state) {
    auto& animation_states = layout_cache_comp->animation_states;
    auto& combat_lanes = layout_cache_comp->combat_lanes;
    auto& visibility_frames = layout_cache_comp->visibility_frames;
    std::size_t const previous_state_count = animation_states.size();
    bool const state_count_changed =
        animation_states.size() != static_cast<std::size_t>(total_layout_count);
    animation_states.resize(static_cast<std::size_t>(total_layout_count));
    combat_lanes.resize(static_cast<std::size_t>(total_layout_count));
    visibility_frames.resize(static_cast<std::size_t>(total_layout_count), 0U);
    if (!preserve_soldier_locomotion) {
      for (auto& state : animation_states) {
        reset_humanoid_locomotion_state(state);
      }
    }
    if (!preserve_soldier_state_prefix) {
      for (auto& lane_state : combat_lanes) {
        lane_state = {};
      }
      std::fill(visibility_frames.begin(), visibility_frames.end(), 0U);
    }
    if (preserve_soldier_locomotion && state_count_changed) {
      for (std::size_t idx = previous_state_count;
           idx < static_cast<std::size_t>(total_layout_count);
           ++idx) {
        reset_humanoid_locomotion_state(animation_states[idx]);
        combat_lanes[idx] = {};
      }
    }
  }
  if (layout_cache_comp != nullptr) {
    auto& ground_samples = layout_cache_comp->ground_samples;
    ground_samples.resize(static_cast<std::size_t>(total_layout_count));
    auto& selection_cache = layout_cache_comp->selection_cache;
    selection_cache.resize(static_cast<std::size_t>(total_layout_count));
    if (!preserve_soldier_state_prefix) {
      std::fill(ground_samples.begin(), ground_samples.end(), SoldierGroundSample{});
      std::fill(
          selection_cache.begin(), selection_cache.end(), SoldierSelectionCache{});
    }
  }

  HumanoidFormationRuntime runtime;
  runtime.layout_cache_comp = layout_cache_comp;
  runtime.soldier_layout_storage = soldier_layout_storage;
  runtime.lod_config = lod_config;
  runtime.allow_animation_persistence = allow_animation_persistence;
  runtime.use_per_soldier_locomotion_state = use_per_soldier_locomotion_state;
  runtime.preserve_soldier_state_prefix = preserve_soldier_state_prefix;
  return runtime;
}

auto resolve_unit_animation_runtime(const HumanoidUnitSnapshot& s,
                                    const HumanoidFormationRuntime& f,
                                    const AnimationInputs& anim,
                                    HumanoidRuntimeContext& runtime)
    -> HumanoidUnitAnimationRuntime {
  using namespace Render::GL;

  const DrawContext& ctx = *s.ctx;
  auto* unit_comp = s.unit_comp;
  auto* transform_comp = s.transform_comp;
  const auto* formation_presentation = s.formation_presentation;
  const auto* creature_presentation = s.creature_presentation;
  const bool has_shared_formation_layout = s.has_shared_formation_layout;
  const bool has_entity_death = s.has_entity_death;
  const int total_layout_count = s.total_layout_count;
  auto* layout_cache_comp = f.layout_cache_comp;
  const bool allow_animation_persistence = f.allow_animation_persistence;
  const QMatrix4x4 k_identity_matrix;

  auto* humanoid_anim_state =
      ctx.entity != nullptr
          ? ctx.entity
                ->get_component<Render::Creature::HumanoidAnimationStateComponent>()
          : nullptr;
  Render::GL::VisualMovementState const visual_movement =
      Render::GL::visual_movement_for_animation_inputs(ctx, anim);
  float const local_enemy_pressure = anim.is_in_melee_lock
                                         ? 1.0F
                                         : (visual_movement.attack_target_in_range
                                                ? 0.7F
                                                : (anim.is_attacking ? 0.35F : 0.0F));
  const HumanoidPreparationModePolicy preparation_mode(ctx.entity);
  const auto commander_jump = preparation_mode.jump_pose();
  const auto rally_pose = preparation_mode.flag_rally_pose();
  float const unit_health_ratio =
      (unit_comp != nullptr && unit_comp->max_health > 0)
          ? std::clamp(static_cast<float>(unit_comp->health) /
                           static_cast<float>(unit_comp->max_health),
                       0.0F,
                       1.0F)
          : 1.0F;
  std::uint32_t unit_attack_target_id = 0U;
  bool unit_attack_target_alive = false;

  if (has_shared_formation_layout && formation_presentation->melee_ordered &&
      formation_presentation->target_alive) {
    unit_attack_target_id = formation_presentation->target_id;
    unit_attack_target_alive = true;
  } else if (creature_presentation != nullptr &&
             creature_presentation->snapshot_valid) {
    unit_attack_target_id = creature_presentation->target_id;
    unit_attack_target_alive = creature_presentation->target_alive;
  } else if (has_shared_formation_layout) {
    unit_attack_target_id = formation_presentation->target_id;
    unit_attack_target_alive = formation_presentation->target_alive;
  }
  auto& animation_diagnostics =
      Render::Profiling::CombatAnimationDiagnostics::instance();
  const bool record_animation_diagnostics =
      animation_diagnostics.enabled() || animation_diagnostics.logging_enabled();

  bool const formation_fight_active = has_shared_formation_layout &&
                                      formation_presentation->melee_ordered &&
                                      formation_presentation->target_alive;

  constexpr float k_formation_fog_radius = 6.0F;
  DrawContext inst_ctx = ctx;

  QVector3D root_position;
  float root_yaw = 0.0F;
  if (transform_comp != nullptr) {
    auto const root =
        Render::Entity::resolve_formation_root(ctx.entity, *transform_comp);
    root_position = root.position;
    root_yaw = root.yaw;
  }

  QMatrix4x4 const unit_base =
      Render::Entity::formation_world_frame(root_position, root_yaw);

  const auto locomotion_override =
      Animation::resolve_humanoid_locomotion_action_override({
          .commander_jump_active = commander_jump.active,
      });

  float turn_smoothing_dt = 0.0F;
  bool turn_smoothing_active = false;
  float turn_smoothing_cap = 2.5F;
  float const ground_march_speed = unit_comp != nullptr ? unit_comp->speed : 2.0F;
  bool turn_smoothing_travel_yaw = true;
  bool turn_smoothing_stagger = true;
  bool turn_smoothing_pivot_wheel = true;
  if (layout_cache_comp != nullptr && allow_animation_persistence &&
      transform_comp != nullptr && !ctx.force_single_soldier && !has_entity_death &&
      total_layout_count > 1) {
    turn_smoothing_active = true;
    auto& turn_cache = *layout_cache_comp;
    if (turn_cache.turn_states.size() != static_cast<std::size_t>(total_layout_count)) {
      turn_cache.turn_states.assign(static_cast<std::size_t>(total_layout_count),
                                    SoldierTurnSmoothingState{});
    }
    if (turn_cache.ground_motion.size() !=
        static_cast<std::size_t>(total_layout_count)) {
      turn_cache.ground_motion.assign(static_cast<std::size_t>(total_layout_count),
                                      SoldierGroundMotionState{});
    }
    if (turn_cache.turn_time_valid) {
      constexpr float k_turn_smoothing_max_dt = 0.1F;
      turn_smoothing_dt =
          std::clamp(anim.time - turn_cache.turn_time, 0.0F, k_turn_smoothing_max_dt);
    }
    turn_cache.turn_time = anim.time;
    turn_cache.turn_time_valid = true;
    float const unit_speed = unit_comp != nullptr ? unit_comp->speed : 2.0F;

    float travel_speed = unit_speed;
    if (Render::Creature::is_running_animation(anim.movement_state)) {
      travel_speed =
          std::max(travel_speed,
                   unit_speed * Engine::Core::StaminaComponent::k_run_speed_multiplier);
    }
    if (ctx.entity != nullptr) {
      if (const auto* motion =
              ctx.entity->get_component<Engine::Core::MotionPresentationComponent>()) {
        travel_speed = std::max(travel_speed, motion->speed);
      }
    }
    turn_smoothing_cap = soldier_catch_up_speed(travel_speed);
    bool const combat_active =
        anim.is_attacking || formation_fight_active || anim.is_in_melee_lock;
    if (combat_active) {
      turn_smoothing_cap *= anim.is_in_melee_lock ? 1.0F : 2.0F;
      turn_smoothing_travel_yaw = false;
      turn_smoothing_stagger = false;
    }
    turn_smoothing_pivot_wheel = !combat_active && !anim.is_in_hold_mode;
  }
  bool const unit_is_archer =
      unit_comp != nullptr && unit_comp->spawn_type == Game::Units::SpawnType::Archer;
  const std::uint32_t ctx_entity_id =
      ctx.entity != nullptr ? static_cast<std::uint32_t>(ctx.entity->get_id()) : 0U;
  const QVector3D unit_origin = ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
  const bool unit_fog_visible =
      ctx.submission_visibility == nullptr ||
      ctx.submission_visibility
          ->evaluate_sphere(unit_origin + QVector3D(0.0F, 0.9F, 0.0F),
                            k_formation_fog_radius,
                            ctx.submission_fog_mode)
          .fog_visible;

  HumanoidUnitAnimationRuntime result;
  result.humanoid_anim_state = humanoid_anim_state;
  result.visual_movement = visual_movement;
  result.local_enemy_pressure = local_enemy_pressure;
  result.commander_jump = commander_jump;
  result.rally_pose = rally_pose;
  result.unit_health_ratio = unit_health_ratio;
  result.unit_attack_target_id = unit_attack_target_id;
  result.unit_attack_target_alive = unit_attack_target_alive;
  result.record_animation_diagnostics = record_animation_diagnostics;
  result.animation_diagnostics = &animation_diagnostics;
  result.formation_fight_active = formation_fight_active;
  result.inst_ctx = inst_ctx;
  result.unit_base = unit_base;
  result.root_position = root_position;
  result.root_yaw = root_yaw;
  result.locomotion_override = locomotion_override;
  result.turn_smoothing_dt = turn_smoothing_dt;
  result.turn_smoothing_active = turn_smoothing_active;
  result.turn_smoothing_cap = turn_smoothing_cap;
  result.ground_march_speed = ground_march_speed;
  result.turn_smoothing_travel_yaw = turn_smoothing_travel_yaw;
  result.turn_smoothing_stagger = turn_smoothing_stagger;
  result.turn_smoothing_pivot_wheel = turn_smoothing_pivot_wheel;
  result.unit_is_archer = unit_is_archer;
  result.ctx_entity_id = ctx_entity_id;
  result.unit_origin = unit_origin;
  result.unit_fog_visible = unit_fog_visible;
  result.stats = &runtime.stats;
  return result;
}

struct CasualtyPose {
  float offset_x{0.0F};
  float offset_y{0.0F};
  float offset_z{0.0F};
  float pitch{0.0F};
  float roll{0.0F};
  float yaw{0.0F};
  const QMatrix4x4* base{nullptr};
  float root_yaw{0.0F};
};

struct SoldierScope {
  const HumanoidUnitSnapshot& unit;
  const HumanoidFormationRuntime& formation;
  HumanoidUnitAnimationRuntime& animation;
  const AnimationInputs& anim;
  std::uint32_t frame_index{0U};
  int idx{0};
  std::size_t slot{0U};
  const Render::Entity::FormationInstance& layout;
  const Engine::Core::FormationSoldierPresentation* presented{nullptr};
  HumanoidInstanceStateComponent* cache{nullptr};
};

auto make_soldier_scope(const HumanoidUnitSnapshot& s,
                        const HumanoidFormationRuntime& f,
                        HumanoidUnitAnimationRuntime& u,
                        const AnimationInputs& anim,
                        std::uint32_t frame_index,
                        int idx) -> SoldierScope {
  auto const slot = static_cast<std::size_t>(idx);
  return SoldierScope{
      .unit = s,
      .formation = f,
      .animation = u,
      .anim = anim,
      .frame_index = frame_index,
      .idx = idx,
      .slot = slot,
      .layout = (*f.soldier_layout_storage)[slot],
      .presented = s.has_shared_formation_layout
                       ? &s.formation_presentation->soldiers[slot]
                       : nullptr,
      .cache = f.layout_cache_comp,
  };
}

struct SoldierDebugFlags {
  bool swing_recoil_active{false};
  bool last_swing_is_melee{false};
  bool combat_ready_stance{false};
};

struct SoldierDebugRoot {
  QVector3D position{};
  float yaw_degrees{0.0F};
  float up_y{1.0F};
  float scale_y{1.0F};
  float tilt_degrees{0.0F};
  float hit_reaction_tilt_degrees{0.0F};
};

struct SoldierDebugGait {
  float locomotion_blend{0.0F};
  float locomotion_presence{0.0F};
  float cycle_phase{0.0F};
  float travel_alignment{1.0F};
  float travel_lateral_share{0.0F};
  float action_link_weight{0.0F};
  bool persistent_valid{false};
  float persistent_last_sample_time{0.0F};
};

struct SoldierDebugRecord {
  float attack_phase{0.0F};
  Render::Creature::AnimationStateId animation_state{};
  HumanoidLOD lod{HumanoidLOD::Culled};
  Render::Profiling::SoldierCullReason cull_reason{
      Render::Profiling::SoldierCullReason::None};
  bool transient_recovery_override{false};
  SoldierDebugRoot root{};
  SoldierDebugGait gait{};
  std::optional<std::uint16_t> animation_clip{};
};

auto debug_gait_of(const HumanoidAnimationContext& anim_ctx) -> SoldierDebugGait {
  SoldierDebugGait gait;
  gait.locomotion_blend = anim_ctx.gait.locomotion_blend;
  gait.locomotion_presence = anim_ctx.gait.locomotion_presence;
  gait.cycle_phase = anim_ctx.gait.cycle_phase;
  gait.travel_alignment = anim_ctx.gait.travel_alignment;
  gait.travel_lateral_share = anim_ctx.gait.turn_amount;
  gait.action_link_weight = anim_ctx.inputs.action_link_weight;
  gait.persistent_valid = anim_ctx.gait.persistent_valid;
  gait.persistent_last_sample_time = anim_ctx.gait.persistent_last_sample_time;
  return gait;
}

auto measure_debug_root(const QMatrix4x4& model,
                        const QVector3D& world_pos,
                        float yaw_degrees,
                        float hit_reaction_tilt_degrees) -> SoldierDebugRoot {
  SoldierDebugRoot root;
  root.position = world_pos;
  root.yaw_degrees = yaw_degrees;
  root.hit_reaction_tilt_degrees = hit_reaction_tilt_degrees;
  QVector3D const model_up = model.mapVector(QVector3D(0.0F, 1.0F, 0.0F));
  root.scale_y = model_up.length();
  QVector3D const normalized_model_up =
      root.scale_y > 1.0e-6F ? model_up / root.scale_y : QVector3D(0.0F, 1.0F, 0.0F);
  root.up_y = std::clamp(normalized_model_up.y(), -1.0F, 1.0F);
  root.tilt_degrees = qRadiansToDegrees(std::acos(root.up_y));
  return root;
}

void record_soldier_debug(const SoldierScope& sc,
                          const AnimationInputs& resolved_anim,
                          const SoldierDebugFlags& flags,
                          const SoldierDebugRecord& record) {
  const DrawContext& ctx = *sc.unit.ctx;
  if (!sc.animation.record_animation_diagnostics || ctx.entity == nullptr) {
    return;
  }

  Render::Profiling::SoldierAnimationDebugSample sample{};
  sample.soldier_index = sc.idx;
  sample.sample_time = resolved_anim.time;
  sample.combat_phase = resolved_anim.combat_phase;
  sample.combat_phase_progress = resolved_anim.combat_phase_progress;
  sample.attack_phase = record.attack_phase;
  sample.attack_variant = resolved_anim.attack_variant;
  sample.is_attacking = resolved_anim.is_attacking;
  sample.attack_is_melee = flags.last_swing_is_melee;
  sample.is_hit_reacting = resolved_anim.is_hit_reacting;
  sample.hit_reaction_kind = resolved_anim.hit_reaction_kind;
  sample.is_swing_recoiling = flags.swing_recoil_active;
  sample.is_in_melee_lock = resolved_anim.is_in_melee_lock;
  sample.transient_recovery_override = record.transient_recovery_override;
  sample.locomotion_state = resolved_anim.movement_state;
  sample.animation_state = record.animation_state;
  sample.animation_clip = record.animation_clip;
  sample.lod = static_cast<std::uint8_t>(record.lod);
  sample.cull_reason = record.cull_reason;
  sample.root_position = record.root.position;
  sample.root_yaw_degrees = record.root.yaw_degrees;
  sample.root_up_y = record.root.up_y;
  sample.root_scale_y = record.root.scale_y;
  sample.root_tilt_degrees = record.root.tilt_degrees;
  sample.hit_reaction_tilt_degrees = record.root.hit_reaction_tilt_degrees;
  sample.locomotion_blend = record.gait.locomotion_blend;
  sample.locomotion_presence = record.gait.locomotion_presence;
  sample.cycle_phase = record.gait.cycle_phase;
  sample.travel_alignment = record.gait.travel_alignment;
  sample.travel_lateral_share = record.gait.travel_lateral_share;
  sample.action_link_weight = record.gait.action_link_weight;
  sample.persistent_valid = record.gait.persistent_valid;
  sample.persistent_last_sample_time = record.gait.persistent_last_sample_time;
  sample.combat_ready_stance = flags.combat_ready_stance;
  sc.animation.animation_diagnostics->record_soldier_sample(ctx.entity->get_id(),
                                                            sample);
}

struct SoldierReaction {
  AnimationInputs anim;
  bool follows_authoritative_strike{true};
  float hit_reaction_base_intensity{0.0F};
  bool swing_recoil_active{false};
};

auto resolve_soldier_reaction(const SoldierScope& sc,
                              const AnimationInputs& soldier_anim) -> SoldierReaction {
  const DrawContext& ctx = *sc.unit.ctx;
  const auto* creature_presentation = sc.unit.creature_presentation;
  const auto* formation_hit = sc.unit.formation_hit;

  SoldierReaction reaction{.anim = soldier_anim};
  AnimationInputs& a = reaction.anim;
  reaction.follows_authoritative_strike =
      sc.presented == nullptr || sc.presented->damage_carrier;
  if (!reaction.follows_authoritative_strike &&
      sc.unit.formation_presentation->melee_ordered) {

    a.has_authored_action_phase = false;
    a.has_authored_action_clip = false;
  }
  if (creature_presentation != nullptr &&
      !creature_presentation->allow_full_body_hit_reaction && a.is_hit_reacting) {

    a.is_hit_reacting = false;
    a.hit_reaction_intensity = 0.0F;
    a.hit_recoil_x = 0.0F;
    a.hit_recoil_z = 0.0F;
  }
  reaction.hit_reaction_base_intensity =
      a.is_hit_reacting
          ? a.hit_reaction_intensity / std::max(0.05F, 1.0F - a.hit_reaction_progress)
          : 0.0F;
  if (formation_hit != nullptr && formation_hit->remaining > 0.0F &&
      formation_hit->soldier_slot == static_cast<std::uint16_t>(sc.idx)) {
    a.is_hit_reacting = true;
    a.hit_reaction_intensity = formation_hit->intensity;
    reaction.hit_reaction_base_intensity = formation_hit->intensity;
    a.hit_reaction_kind = formation_hit->reaction_kind;
    float const hit_duration = formation_hit->duration > 0.0F
                                   ? formation_hit->duration
                                   : std::max(formation_hit->remaining, 0.28F);
    a.hit_reaction_progress =
        std::clamp(1.0F - formation_hit->remaining / hit_duration, 0.0F, 1.0F);
    a.hit_recoil_x = formation_hit->hit_direction_x;
    a.hit_recoil_z = formation_hit->hit_direction_z;
    if (std::abs(a.hit_recoil_x) < 1.0e-4F && std::abs(a.hit_recoil_z) < 1.0e-4F &&
        ctx.entity != nullptr) {
      if (auto const* feedback =
              ctx.entity->get_component<Engine::Core::HitFeedbackComponent>()) {
        a.hit_recoil_x = feedback->hit_direction_x;
        a.hit_recoil_z = feedback->hit_direction_z;
      }
    }
  }

  bool const authored_swing_owns_body = a.has_authored_action_phase && a.is_attacking;
  reaction.swing_recoil_active =
      (authored_swing_owns_body ||
       a.hit_reaction_kind == Engine::Core::HitReactionKind::Recoil) &&
      a.is_hit_reacting;
  if (reaction.swing_recoil_active) {
    a.is_hit_reacting = false;
  }
  return reaction;
}

void apply_guard_shield_pose(const SoldierScope& sc, AnimationInputs& a) {
  using namespace Render::GL;
  if (sc.unit.is_mounted_spawn || guard_pose_amount(a) <= 0.0F ||
      a.shield_formation_pose != ShieldFormationPose::None) {
    return;
  }
  const auto* creature_presentation = sc.unit.creature_presentation;
  a.shield_formation_pose = shared_guard_shield_pose(
      sc.unit.unit_comp,
      sc.unit.visual_spec,
      creature_presentation != nullptr && creature_presentation->formation_guard_active,
      creature_presentation != nullptr && creature_presentation->guard_requested,
      creature_presentation != nullptr &&
          creature_presentation->defensive_layout_locked,
      static_cast<int>(sc.layout.row_index),
      static_cast<int>(sc.layout.col_index),
      sc.unit.rows,
      sc.unit.cols);
}

struct SoldierPlacement {
  QMatrix4x4 model{};
  QVector3D world_pos{};
  float yaw_degrees{0.0F};
  float entity_ground_offset{0.0F};
  SoldierTurnSmoothingResult turn_smoothing{};
  QVector3D turn_slot_world{};
  SoldierGroundMotion ground_motion{};
  bool has_shared_footsteps{false};
  bool elevated{false};
  bool casualty_body{false};
};

auto resolve_soldier_footing(const SoldierScope& sc,
                             float applied_yaw_offset,
                             const AnimationInputs& a,
                             SoldierPlacement& p) -> bool {
  const auto& s = sc.unit;
  const auto& u = sc.animation;
  auto* cache = sc.cache;

  if (p.has_shared_footsteps) {
    auto const& footing = *sc.presented;

    p.turn_smoothing.x = footing.world_x;
    p.turn_smoothing.z = footing.world_z;
    p.turn_smoothing.yaw_degrees = footing.world_yaw;
    p.turn_smoothing.travel_speed =
        std::hypot(footing.world_velocity_x, footing.world_velocity_z);
    p.turn_smoothing.travel_yaw_degrees =
        p.turn_smoothing.travel_speed > 0.01F
            ? std::atan2(footing.world_velocity_x, footing.world_velocity_z) * 180.0F /
                  std::numbers::pi_v<float>
            : footing.world_yaw;
    p.turn_smoothing.relocating = footing.reforming;
    if (u.turn_smoothing_active && sc.slot < cache->turn_states.size()) {
      auto const eased = ease_melee_footing(cache->turn_states[sc.slot],
                                            footing.world_x,
                                            footing.world_z,
                                            u.turn_smoothing_dt,
                                            footing.opponent_id != 0U,
                                            sc.frame_index + 1U);
      p.turn_smoothing.x = eased.x;
      p.turn_smoothing.z = eased.z;
    }
    p.turn_slot_world =
        QVector3D(p.turn_smoothing.x, u.root_position.y(), p.turn_smoothing.z);
    return true;
  }

  if (!u.turn_smoothing_active || p.casualty_body || a.simulation_owns_root_motion ||
      sc.slot >= cache->turn_states.size()) {
    return false;
  }

  p.turn_slot_world =
      u.unit_base.map(QVector3D(sc.layout.offset_x, 0.0F, sc.layout.offset_z));

  auto const turn_variation =
      soldier_turn_variation(sc.layout.inst_seed,
                             static_cast<int>(sc.layout.row_index),
                             s.rows,
                             s.is_mounted_spawn);
  SoldierTurnSmoothingInputs smoothing_inputs{};
  smoothing_inputs.target_x = p.turn_slot_world.x();
  smoothing_inputs.target_z = p.turn_slot_world.z();
  smoothing_inputs.formation_yaw_degrees = u.root_yaw + applied_yaw_offset;
  smoothing_inputs.formation_center_x = u.root_position.x();
  smoothing_inputs.formation_center_z = u.root_position.z();
  smoothing_inputs.dt = u.turn_smoothing_dt;
  smoothing_inputs.max_speed =
      u.turn_smoothing_cap * turn_variation.catch_up_speed_scale;
  smoothing_inputs.turn_rate_degrees =
      (s.is_mounted_spawn ? 150.0F : 200.0F) * turn_variation.turn_rate_scale;
  smoothing_inputs.response_delay_seconds =
      u.turn_smoothing_stagger ? turn_variation.response_delay_seconds : 0.0F;
  smoothing_inputs.allow_travel_yaw = u.turn_smoothing_travel_yaw;
  smoothing_inputs.position_is_authoritative = s.has_shared_formation_layout;
  smoothing_inputs.allow_pivot_wheel = u.turn_smoothing_pivot_wheel;

  smoothing_inputs.frame_index = sc.frame_index + 1U;
  p.turn_smoothing =
      resolve_soldier_turn_smoothing(cache->turn_states[sc.slot], smoothing_inputs);
  return true;
}

void build_soldier_model(const SoldierScope& sc,
                         const CasualtyPose& casualty,
                         float applied_yaw_offset,
                         bool turn_smoothed,
                         SoldierPlacement& p) {
  const DrawContext& ctx = *sc.unit.ctx;
  const auto* transform_comp = sc.unit.transform_comp;
  const auto& u = sc.animation;
  float const offset_x = sc.layout.offset_x;
  float const offset_z = sc.layout.offset_z;

  p.yaw_degrees = applied_yaw_offset;
  if (transform_comp != nullptr && turn_smoothed) {
    p.yaw_degrees = p.turn_smoothing.yaw_degrees;
    QMatrix4x4 m;
    m.translate(p.turn_smoothing.x, u.root_position.y(), p.turn_smoothing.z);
    m.rotate(p.turn_smoothing.yaw_degrees, 0.0F, 1.0F, 0.0F);
    m.scale(transform_comp->scale.x, transform_comp->scale.y, transform_comp->scale.z);
    p.model = m;
  } else if (transform_comp != nullptr) {
    float const body_root_yaw =
        casualty.base != nullptr ? casualty.root_yaw : u.root_yaw;
    p.yaw_degrees = body_root_yaw + applied_yaw_offset;
    QMatrix4x4 m = casualty.base != nullptr ? *casualty.base : u.unit_base;
    m.translate(
        offset_x + casualty.offset_x, casualty.offset_y, offset_z + casualty.offset_z);
    m.rotate(applied_yaw_offset, 0.0F, 1.0F, 0.0F);
    if (std::abs(casualty.pitch) > 0.01F || std::abs(casualty.roll) > 0.01F) {

      constexpr float k_casualty_tumble_pivot_y = 0.92F;
      m.translate(0.0F, k_casualty_tumble_pivot_y, 0.0F);
      m.rotate(casualty.pitch, 1.0F, 0.0F, 0.0F);
      m.rotate(casualty.roll, 0.0F, 0.0F, 1.0F);
      m.translate(0.0F, -k_casualty_tumble_pivot_y, 0.0F);
    }

    m.scale(transform_comp->scale.x, transform_comp->scale.y, transform_comp->scale.z);
    p.model = m;
  } else {
    p.model = ctx.model;
    p.model.translate(offset_x, 0.0F, offset_z);
    p.model.rotate(p.yaw_degrees, 0.0F, 1.0F, 0.0F);
  }

  if (ctx.submission_visibility != nullptr) {
    QVector3D const push = ctx.submission_visibility->focus_body_push(
        p.model.map(QVector3D(0.0F, 0.0F, 0.0F)), u.ctx_entity_id);
    if (!push.isNull()) {
      QMatrix4x4 clear_of_focus;
      clear_of_focus.translate(push);
      p.model = clear_of_focus * p.model;
    }
  }
  p.world_pos = p.model.map(QVector3D(0.0F, 0.0F, 0.0F));
}

void observe_soldier_steps(const SoldierScope& sc,
                           bool turn_smoothed,
                           SoldierPlacement& p,
                           AnimationInputs& a) {
  auto* cache = sc.cache;
  auto const* footing = sc.presented;

  if (turn_smoothed) {
    SoldierGroundMotionInputs ground_inputs{};
    ground_inputs.x = p.world_pos.x();
    ground_inputs.z = p.world_pos.z();
    ground_inputs.yaw_degrees = p.yaw_degrees;
    ground_inputs.time = sc.anim.time;
    ground_inputs.march_speed = sc.animation.ground_march_speed;
    float const declared_rad = qDegreesToRadians(p.turn_smoothing.travel_yaw_degrees);
    ground_inputs.seed_velocity_x =
        std::sin(declared_rad) * p.turn_smoothing.travel_speed;
    ground_inputs.seed_velocity_z =
        std::cos(declared_rad) * p.turn_smoothing.travel_speed;
    ground_inputs.carried =
        p.has_shared_footsteps && (footing->carried || footing->climbing);
    SoldierGroundMotionState* ground_state = nullptr;
    if (cache != nullptr && sc.slot < cache->ground_motion.size()) {
      ground_state = &cache->ground_motion[sc.slot];
    }
    if (ground_state != nullptr && sc.formation.allow_animation_persistence) {
      p.ground_motion = observe_soldier_ground_motion(*ground_state, ground_inputs);
    } else {
      p.ground_motion = peek_soldier_ground_motion(
          ground_state != nullptr ? *ground_state : SoldierGroundMotionState{},
          ground_inputs);
    }
    if (p.has_shared_footsteps && p.ground_motion.stepping) {
      a.is_constructing = false;
    }
    if (!a.is_constructing) {
      a.movement_state = p.ground_motion.running    ? Animation::MovementState::Run
                         : p.ground_motion.stepping ? Animation::MovementState::Walk
                                                    : Animation::MovementState::Idle;
      a.visual_movement.measured_from_ground = true;
    }
  }

  if (p.has_shared_footsteps && footing->climbing) {
    a.is_constructing = true;
    a.construction_job = static_cast<std::uint8_t>(Animation::HumanoidWorkJob::Climb);
    a.construction_progress =
        footing->elevation / Animation::k_humanoid_climb_rise_per_cycle;
    a.is_attacking = false;
    a.movement_state = Animation::MovementState::Idle;
    a.visual_movement.measured_from_ground = false;
  }
}

auto place_soldier(const SoldierScope& sc,
                   const CasualtyPose& casualty,
                   AnimationInputs& a) -> SoldierPlacement {
  SoldierPlacement p;
  float const applied_yaw_offset = sc.layout.yaw_offset + casualty.yaw;
  p.entity_ground_offset = sc.unit.entity_ground_offset;
  p.casualty_body = a.is_dying || a.is_dead;
  if (sc.presented != nullptr) {
    p.elevated = sc.presented->elevation > 0.0F;

    p.entity_ground_offset = -sc.presented->elevation;
  }
  p.has_shared_footsteps = sc.presented != nullptr &&
                           sc.presented->world_motion_valid && !p.casualty_body &&
                           !a.simulation_owns_root_motion;

  bool const turn_smoothed = resolve_soldier_footing(sc, applied_yaw_offset, a, p);
  build_soldier_model(sc, casualty, applied_yaw_offset, turn_smoothed, p);
  observe_soldier_steps(sc, turn_smoothed, p, a);
  return p;
}

struct SoldierVisibility {
  bool outside_frustum{false};
  bool hidden_by_fog{false};
  float lens_gap_visibility{1.0F};

  [[nodiscard]] auto blocks_lens_gap() const noexcept -> bool {
    return lens_gap_visibility <= 0.0F;
  }
  [[nodiscard]] auto culled() const noexcept -> bool {
    return outside_frustum || hidden_by_fog || blocks_lens_gap();
  }
  [[nodiscard]] auto
  cull_reason() const noexcept -> Render::Profiling::SoldierCullReason {
    using Reason = Render::Profiling::SoldierCullReason;
    return outside_frustum ? Reason::Frustum
           : hidden_by_fog ? Reason::Fog
                           : Reason::LensGap;
  }
};

auto evaluate_soldier_visibility(const SoldierScope& sc,
                                 const SoldierPlacement& p) -> SoldierVisibility {
  using namespace Render::GL;
  const DrawContext& ctx = *sc.unit.ctx;
  auto* cache = sc.cache;

  constexpr float k_soldier_cull_center_y = 0.9F;
  constexpr float k_soldier_cull_radius = 1.4F;
  const QVector3D cull_center =
      p.model.map(QVector3D(0.0F, k_soldier_cull_center_y, 0.0F));

  const std::uint32_t encoded_visible_frame =
      cache != nullptr && sc.slot < cache->visibility_frames.size()
          ? cache->visibility_frames[sc.slot]
          : 0U;
  const bool recently_drawn =
      encoded_visible_frame != 0U && (sc.frame_index - (encoded_visible_frame - 1U)) <=
                                         k_soldier_visibility_memory_frames;
  const float cull_radius =
      k_soldier_cull_radius +
      (recently_drawn ? k_soldier_cull_keep_band : k_soldier_cull_enter_band);

  const auto visibility_result =
      ctx.submission_visibility != nullptr
          ? ctx.submission_visibility->evaluate_sphere_with_margin(
                cull_center, cull_radius, SubmissionFogMode::Ignore)
          : SubmissionVisibilityResult{
                ctx.camera == nullptr ||
                    ctx.camera->is_in_frustum(cull_center, cull_radius),
                true};

  SoldierVisibility visibility;
  visibility.outside_frustum = !visibility_result.in_frustum;
  visibility.hidden_by_fog = !sc.animation.unit_fog_visible;
  visibility.lens_gap_visibility =
      ctx.submission_visibility != nullptr
          ? ctx.submission_visibility->lens_gap_visibility(
                p.world_pos,
                sc.animation.ctx_entity_id,
                sc.unit.is_mounted_spawn
                    ? SubmissionVisibilityPolicy::k_mounted_body_radius
                    : SubmissionVisibilityPolicy::k_default_body_radius)
          : 1.0F;
  return visibility;
}

void count_soldier_cull(Render::GL::HumanoidRenderStats& stats,
                        const SoldierVisibility& visibility) {
  if (visibility.outside_frustum) {
    ++stats.soldiers_skipped_frustum;
  } else if (visibility.hidden_by_fog) {
    ++stats.soldiers_skipped_fog;
  } else {
    ++stats.soldiers_skipped_lens_gap;
  }
}

struct SoldierBasis {
  VariationParams variation;
  float combined_height_scale{1.0F};
  float yaw_radians{0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  VisualLocomotionSample visual_locomotion{};
};

auto resolve_soldier_basis(const SoldierScope& sc,
                           const SoldierPlacement& p,
                           const AnimationInputs& a,
                           DrawContext& inst_ctx) -> SoldierBasis {
  const auto& s = sc.unit;
  auto const* humanoid_anim_state = sc.animation.humanoid_anim_state;
  std::uint32_t const inst_seed = sc.layout.inst_seed;

  SoldierBasis basis{.variation = VariationParams::from_seed(inst_seed)};
  VariationParams& variation = basis.variation;
  s.owner->adjust_variation(inst_ctx, inst_seed, variation);
  auto const locomotion_variation = Animation::resolve_humanoid_locomotion_variation({
      .has_locomotion = Render::Creature::is_moving_animation(a.movement_state),
      .running = Render::Creature::is_running_animation(a.movement_state),

      .blend_from_presence =
          a.simulation_owns_root_motion && humanoid_anim_state != nullptr,
      .locomotion_presence = humanoid_anim_state != nullptr
                                 ? humanoid_anim_state->locomotion.locomotion_presence
                                 : 0.0F,
      .run_presence = humanoid_anim_state != nullptr
                          ? humanoid_anim_state->locomotion.run_presence
                          : 0.0F,
      .walk_speed_multiplier = variation.walk_speed_mult,
      .arm_swing_amplitude = variation.arm_swing_amp,
      .stance_width = variation.stance_width,
      .posture_slump = variation.posture_slump,
  });
  variation.walk_speed_mult = locomotion_variation.walk_speed_multiplier;
  variation.arm_swing_amp = locomotion_variation.arm_swing_amplitude;
  variation.stance_width = locomotion_variation.stance_width;
  variation.posture_slump = locomotion_variation.posture_slump;

  basis.combined_height_scale = s.height_scale * variation.height_scale;
  if (s.needs_height_scaling || std::abs(variation.height_scale - 1.0F) > 0.001F) {
    QMatrix4x4 scale_matrix;
    scale_matrix.scale(variation.bulk_scale, basis.combined_height_scale, 1.0F);
    inst_ctx.model = inst_ctx.model * scale_matrix;
  }
  basis.yaw_radians = qDegreesToRadians(p.yaw_degrees);
  QVector3D forward(std::sin(basis.yaw_radians), 0.0F, std::cos(basis.yaw_radians));
  if (forward.lengthSquared() > 1e-8F) {
    forward.normalize();
  } else {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  QVector3D right = QVector3D::crossProduct(basis.up, forward);
  if (right.lengthSquared() > 1e-8F) {
    right.normalize();
  } else {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  }
  basis.forward = forward;
  basis.right = right;
  basis.visual_locomotion =
      resolve_visual_locomotion_sample(sc.animation.visual_movement, forward);
  return basis;
}

struct SoldierPersistentState {
  Render::Creature::HumanoidAnimationStateComponent* animation{nullptr};
  Render::Creature::SoldierCombatLaneState* lane{nullptr};
};

auto resolve_soldier_persistent_state(const SoldierScope& sc)
    -> SoldierPersistentState {
  SoldierPersistentState persistent{.animation = sc.animation.humanoid_anim_state};
  auto* cache = sc.cache;
  if (cache != nullptr && sc.formation.use_per_soldier_locomotion_state) {
    if (sc.slot < cache->animation_states.size()) {
      persistent.animation = &cache->animation_states[sc.slot];
    }
    if (sc.slot < cache->combat_lanes.size()) {
      persistent.lane = &cache->combat_lanes[sc.slot];
    }
  }
  return persistent;
}

struct SoldierFormationFight {
  bool formation_melee{false};
  bool in_formation_fight{false};
};

auto preferred_lane_for_role(Engine::Core::FormationSoldierCombatRole role,
                             Render::Creature::SoldierCombatLane fallback)
    -> Render::Creature::SoldierCombatLane {
  using CoreRole = Engine::Core::FormationSoldierCombatRole;
  using Lane = Render::Creature::SoldierCombatLane;
  switch (role) {
  case CoreRole::LeadStrike:
    return Lane::LeadStrike;
  case CoreRole::SupportStrike:
    return Lane::SupportStrike;
  case CoreRole::Guard:
    return Lane::ShieldBrace;
  case CoreRole::StepIn:
    return Lane::StepIn;
  case CoreRole::StepOut:
    return Lane::StepOut;
  case CoreRole::Ready:
    return Lane::IdleReady;
  case CoreRole::None:
    break;
  }
  return fallback;
}

auto resolve_soldier_lane(const SoldierScope& sc,
                          const SoldierPersistentState& persistent,
                          const AnimationInputs& a,
                          const SoldierFormationFight& fight)
    -> Render::Creature::CombatLaneProfile {
  const DrawContext& ctx = *sc.unit.ctx;
  const auto& s = sc.unit;
  auto const* directive = sc.presented;

  Render::Creature::CombatLaneInputs lane_inputs{};
  lane_inputs.unit_seed = s.seed;
  lane_inputs.soldier_seed = sc.layout.inst_seed;
  lane_inputs.row = static_cast<int>(sc.layout.row_index);
  lane_inputs.col = static_cast<int>(sc.layout.col_index);
  lane_inputs.rows = s.rows;
  lane_inputs.cols = s.cols;
  lane_inputs.health_ratio = sc.animation.unit_health_ratio;
  lane_inputs.local_enemy_pressure = fight.formation_melee
                                         ? (directive->opponent_id != 0U ? 1.0F : 0.15F)
                                         : sc.animation.local_enemy_pressure;
  lane_inputs.force_single_soldier = ctx.force_single_soldier;

  lane_inputs.is_melee = a.is_melee || fight.in_formation_fight;
  lane_inputs.is_mounted = s.is_mounted_spawn;
  lane_inputs.attack_family = a.attack_family;

  Render::Creature::SoldierCombatLaneState const lane_snapshot =
      (persistent.lane != nullptr) ? *persistent.lane
                                   : Render::Creature::SoldierCombatLaneState{};
  auto lane_resolution =
      Render::Creature::resolve_soldier_combat_lane(lane_snapshot, lane_inputs);
  if (fight.in_formation_fight) {
    auto preferred_lane =
        preferred_lane_for_role(directive->combat_role, lane_resolution.profile.lane);
    if (directive->damage_carrier) {
      preferred_lane = Render::Creature::SoldierCombatLane::LeadStrike;
    }
    lane_resolution.state.lane = preferred_lane;
    lane_resolution.profile =
        Animation::profile_with_lane(lane_resolution.profile, preferred_lane);
    lane_resolution.profile.phase_bias += directive->combat_phase_bias;
  }
  if (sc.formation.allow_animation_persistence && persistent.lane != nullptr) {
    *persistent.lane = lane_resolution.state;
  }
  return lane_resolution.profile;
}

auto resolve_visual_attack_request(
    const SoldierScope& sc,
    const Render::Creature::CombatVisualPersistentState& previous,
    const SoldierFormationFight& fight,
    bool lone_lock_attack,
    AnimationInputs& a) -> bool {
  auto const* directive = sc.presented;
  bool requested = a.is_attacking || fight.in_formation_fight;
  if (lone_lock_attack) {

    requested = false;
    a.movement_state = Render::Creature::MovementAnimationState::Idle;
  }
  if (fight.in_formation_fight && !directive->damage_carrier) {
    using Role = Engine::Core::FormationSoldierCombatRole;
    requested = directive->combat_role == Role::LeadStrike ||
                directive->combat_role == Role::SupportStrike ||
                directive->combat_role == Role::StepIn;
  }
  if (fight.in_formation_fight && !requested && !a.is_hit_reacting && previous.active &&
      !previous.strike_committed && !previous.aborting &&
      previous.exit_policy == Animation::CombatExitPolicy::None &&
      (directive->opponent_id == 0U ||
       directive->opponent_id == previous.source_target_id)) {
    requested = true;
  }
  return requested;
}

auto build_raw_combat_inputs(const SoldierScope& sc,
                             const AnimationInputs& a,
                             const SoldierReaction& reaction,
                             const SoldierFormationFight& fight,
                             bool attack_requested,
                             std::uint8_t base_attack_variant)
    -> Render::Creature::CombatVisualRawInputs {
  auto const* directive = sc.presented;
  auto const& individuality = sc.layout.individuality;
  bool const directed_opponent = directive != nullptr && directive->opponent_id != 0U;

  Render::Creature::CombatVisualRawInputs raw_combat{};

  raw_combat.sample_time =
      a.time * (directive != nullptr ? directive->combat_speed_scale : 1.0F) *
      individuality.swing_tempo_scale;

  raw_combat.attack_offset = a.attack_offset + individuality.swing_phase_offset;
  raw_combat.has_attack_offset = true;
  raw_combat.attack_requested = attack_requested;
  raw_combat.is_melee = a.is_melee || fight.in_formation_fight;
  raw_combat.is_mounted = sc.unit.is_mounted_spawn;
  raw_combat.is_casting = a.is_casting;
  raw_combat.finisher_attack = a.finisher_attack;
  raw_combat.amplified_attack = false;
  raw_combat.is_hit_reacting = a.is_hit_reacting;
  raw_combat.is_healing = a.is_healing;
  raw_combat.is_constructing = a.is_constructing;
  raw_combat.is_dying = a.is_dying;
  raw_combat.is_dead = a.is_dead;
  raw_combat.locomotion = a.movement_state;
  raw_combat.combat_phase = reaction.follows_authoritative_strike
                                ? a.combat_phase
                                : Render::GL::CombatAnimPhase::Idle;
  raw_combat.combat_phase_progress =
      reaction.follows_authoritative_strike ? a.combat_phase_progress : 0.0F;
  raw_combat.attack_variant = base_attack_variant;
  raw_combat.attack_target_id =
      directed_opponent ? directive->opponent_id : sc.animation.unit_attack_target_id;
  raw_combat.attack_target_alive =
      directed_opponent ? true : sc.animation.unit_attack_target_alive;
  raw_combat.attack_family = a.attack_family;
  raw_combat.authored_action_phase = a.authored_action_phase;
  raw_combat.has_authored_action_phase = a.has_authored_action_phase;
  return raw_combat;
}

auto resolve_soldier_combat(const SoldierScope& sc,
                            const SoldierPersistentState& persistent,
                            SoldierReaction& reaction) -> bool {
  using namespace Render::GL;
  const DrawContext& ctx = *sc.unit.ctx;
  AnimationInputs& a = reaction.anim;
  auto const* directive = sc.presented;

  std::uint8_t base_attack_variant = a.attack_variant;
  if (ctx.has_attack_variant_override) {
    base_attack_variant = ctx.attack_variant_override;
  }

  SoldierFormationFight fight;
  fight.formation_melee =
      directive != nullptr && sc.unit.formation_presentation->melee_ordered;
  fight.in_formation_fight =
      sc.animation.formation_fight_active && directive != nullptr && directive->alive;

  auto const lane_profile = resolve_soldier_lane(sc, persistent, a, fight);

  Render::Creature::CombatVisualPersistentState previous_combat_visual{};
  if (persistent.animation != nullptr) {
    previous_combat_visual = persistent.animation->combat_visual;
  }

  if (a.is_hit_reacting &&
      a.hit_reaction_kind != Engine::Core::HitReactionKind::Stagger &&
      previous_combat_visual.active &&
      (previous_combat_visual.phase == Animation::CombatTransactionPhase::Strike ||
       previous_combat_visual.phase ==
           Animation::CombatTransactionPhase::FollowThrough)) {
    reaction.swing_recoil_active = true;
    a.is_hit_reacting = false;
  }

  bool const lock_only_attack = a.attack_from_melee_lock &&
                                a.combat_phase == CombatAnimPhase::Idle &&
                                !a.has_authored_action_phase;
  bool const single_body_unit =
      ctx.force_single_soldier || sc.unit.total_layout_count <= 1;
  bool const lone_lock_attack = lock_only_attack && single_body_unit &&
                                directive == nullptr && !fight.in_formation_fight;
  bool const attack_requested = resolve_visual_attack_request(
      sc, previous_combat_visual, fight, lone_lock_attack, a);
  auto const raw_combat = build_raw_combat_inputs(
      sc, a, reaction, fight, attack_requested, base_attack_variant);

  auto combat_resolution = Render::Creature::resolve_combat_visual_state(
      previous_combat_visual, raw_combat, lane_profile);
  if (fight.formation_melee || lone_lock_attack) {
    combat_resolution.resolved.authoritative = true;
  }
  if (sc.formation.allow_animation_persistence && persistent.animation != nullptr) {
    persistent.animation->combat_visual = combat_resolution.persistent;
  }
  bool last_swing_is_melee = false;
  {
    auto const& shown = combat_resolution.resolved;
    auto const& swing = combat_resolution.persistent;
    last_swing_is_melee = shown.active                  ? shown.is_melee
                          : swing.aborted_phase >= 0.0F ? swing.aborted_is_melee
                                                        : raw_combat.is_melee;
  }
  sync_combat_visual_inputs(a, combat_resolution.resolved);
  if (fight.in_formation_fight && !a.is_attacking && !a.is_hit_reacting) {

    if (directive->combat_role == Engine::Core::FormationSoldierCombatRole::Guard) {
      a.is_guarding = true;
      a.guard_pose_progress =
          std::clamp(directive->target_held_seconds / 0.18F, 0.0F, 1.0F);
    } else if (a.visual_movement.measured_from_ground ||
               !Render::Creature::is_moving_animation(a.movement_state)) {
      a.is_in_melee_lock = true;
    }
  }
  apply_guard_shield_pose(sc, a);

  a.melee_intent = Animation::melee_intent_rotated(
      a.melee_intent, sc.layout.individuality.swing_plane_offset);
  return last_swing_is_melee;
}

auto resolve_soldier_locomotion(const SoldierScope& sc,
                                const SoldierPlacement& p,
                                const SoldierBasis& basis,
                                const SoldierPersistentState& persistent,
                                const AnimationInputs& a) -> HumanoidLocomotionState {
  const auto* transform_comp = sc.unit.transform_comp;

  HumanoidLocomotionInputs locomotion_inputs{};
  locomotion_inputs.anim = a;
  locomotion_inputs.variation = basis.variation;
  locomotion_inputs.move_speed = basis.visual_locomotion.speed;
  if (transform_comp != nullptr && transform_comp->scale.x > 1.0e-3F) {
    locomotion_inputs.body_scale = transform_comp->scale.x;
  }
  locomotion_inputs.entity_forward = basis.forward;
  locomotion_inputs.locomotion_direction = basis.visual_locomotion.direction;
  locomotion_inputs.movement_target = basis.visual_locomotion.movement_target;
  locomotion_inputs.has_movement_target = basis.visual_locomotion.has_movement_target;
  locomotion_inputs.animation_time = sc.anim.time;
  locomotion_inputs.individuality = sc.layout.individuality;
  locomotion_inputs.persistent_state = persistent.animation;
  locomotion_inputs.allow_persistent_update = sc.formation.allow_animation_persistence;
  if (a.visual_movement.measured_from_ground) {

    locomotion_inputs.move_speed = p.ground_motion.stride_speed;
    locomotion_inputs.stride_presence = ground_motion_stride_presence(
        p.ground_motion.stride_speed / locomotion_inputs.body_scale,
        p.ground_motion.running);
    locomotion_inputs.locomotion_direction =
        p.ground_motion.travelling
            ? QVector3D(p.ground_motion.travel_x, 0.0F, p.ground_motion.travel_z)
            : basis.forward;
    locomotion_inputs.movement_target = p.turn_slot_world;
    locomotion_inputs.has_movement_target =
        !p.has_shared_footsteps && p.turn_smoothing.relocating;
  }
  HumanoidLocomotionState locomotion_state =
      build_humanoid_locomotion_state(locomotion_inputs);

  const auto& locomotion_override = sc.animation.locomotion_override;
  if (locomotion_override.active) {
    locomotion_state.move_speed = locomotion_override.move_speed;
    locomotion_state.has_movement_target = locomotion_override.has_target;
    locomotion_state.locomotion_velocity = QVector3D(0.0F, 0.0F, 0.0F);
    locomotion_state.gait.state = locomotion_override.state;
    locomotion_state.gait.speed = locomotion_override.move_speed;
    locomotion_state.gait.normalized_speed = locomotion_override.normalized_speed;
    locomotion_state.gait.velocity = QVector3D(0.0F, 0.0F, 0.0F);
    locomotion_state.gait.has_target = locomotion_override.has_target;
    locomotion_state.gait.is_airborne = locomotion_override.airborne;
  }
  return locomotion_state;
}

struct SoldierConstruction {
  Render::GL::ConstructionRole role{Render::GL::ConstructionRole::None};
  float blend{0.0F};
};

auto resolve_soldier_construction(const SoldierScope& sc,
                                  const SoldierPersistentState& persistent,
                                  AnimationInputs& a) -> SoldierConstruction {
  using namespace Render::GL;
  ConstructionRole construction_role = ConstructionRole::None;
  if (a.is_constructing) {
    construction_role = resolve_construction_role(sc.unit.visual_spec,
                                                  sc.layout.inst_seed,
                                                  sc.unit.ctx->force_single_soldier,
                                                  a.construction_job);
  }
  Animation::HumanoidConstructionTransitionSample construction_transition{};
  if (persistent.animation != nullptr) {
    construction_transition = Animation::resolve_humanoid_construction_transition({
        .state = persistent.animation->construction_transition,
        .constructing = a.is_constructing,
        .sample_time = sc.anim.time,
        .construction_phase = a.construction_progress,
        .role = construction_role,
    });
    if (sc.formation.allow_animation_persistence) {
      persistent.animation->construction_transition = construction_transition.state;
    }
  } else if (a.is_constructing) {
    construction_transition.pose_weight = 1.0F;
    construction_transition.phase = a.construction_progress;
    construction_transition.role = construction_role;
  }
  if (construction_transition.active()) {
    construction_role = construction_transition.role;
    a.construction_progress = construction_transition.phase;
  }
  return {.role = construction_role, .blend = construction_transition.pose_weight};
}

void apply_soldier_ambient_idle(const SoldierScope& sc,
                                const SoldierPersistentState& persistent,
                                bool render_has_locomotion,
                                HumanoidAnimationContext& anim_ctx) {
  const auto& u = sc.animation;
  Animation::HumanoidAmbientRuntimeState previous_ambient{};
  if (persistent.animation != nullptr) {
    previous_ambient = persistent.animation->ambient_idle;
  }
  Animation::HumanoidAmbientSelectionInputs ambient_inputs{
      .ambient_state = previous_ambient,
      .sample_time = sc.anim.time,
      .jump_active = u.commander_jump.active,
      .jump_phase = u.commander_jump.phase,
      .flag_rally_active = u.rally_pose.active,
      .flag_rally_phase = u.rally_pose.phase,
      .mounted = sc.unit.is_mounted_spawn,
      .has_locomotion = render_has_locomotion,
      .attacking = anim_ctx.inputs.is_attacking,
      .in_hold_mode = anim_ctx.inputs.is_in_hold_mode,
      .guarding = anim_ctx.inputs.is_guarding,
      .exiting_guard = anim_ctx.inputs.is_exiting_guard,
      .constructing = anim_ctx.inputs.is_constructing,
      .healing = anim_ctx.inputs.is_healing,
      .hit_reacting = anim_ctx.inputs.is_hit_reacting,
      .dying = anim_ctx.inputs.is_dying,
      .dead = anim_ctx.inputs.is_dead,
      .routing = anim_ctx.inputs.is_routing,
      .seed = sc.layout.inst_seed,
      .idle_duration = anim_ctx.inputs.idle_duration,
  };
  auto const ambient_tick =
      Animation::resolve_humanoid_ambient_selection(ambient_inputs);

  if (sc.formation.allow_animation_persistence && persistent.animation != nullptr) {
    persistent.animation->ambient_idle = ambient_tick.state;
  }
  if (ambient_tick.sample.active) {
    anim_ctx.ambient_idle_type = ambient_tick.sample.type;
    anim_ctx.ambient_idle_phase = ambient_tick.sample.phase;
    anim_ctx.ambient_idle_blend = ambient_tick.sample.blend;
  }
}

auto build_soldier_animation_context(const SoldierScope& sc,
                                     const SoldierPlacement& p,
                                     const SoldierBasis& basis,
                                     const SoldierPersistentState& persistent,
                                     bool render_has_locomotion,
                                     AnimationInputs& a) -> HumanoidAnimationContext {
  const auto& individuality = sc.layout.individuality;
  HumanoidLocomotionState const locomotion_state =
      resolve_soldier_locomotion(sc, p, basis, persistent, a);
  SoldierConstruction const construction =
      resolve_soldier_construction(sc, persistent, a);

  HumanoidAnimationContext anim_ctx{};
  anim_ctx.inputs = a;
  anim_ctx.inputs.is_mounted = sc.unit.is_mounted_spawn;
  anim_ctx.variation = basis.variation;
  anim_ctx.formation = sc.unit.formation;
  anim_ctx.individuality = individuality;
  anim_ctx.jitter_seed = individuality.idle_phase_offset;

  anim_ctx.entity_forward = basis.forward;
  anim_ctx.entity_right = basis.right;
  anim_ctx.entity_up = basis.up;
  anim_ctx.locomotion_direction = locomotion_state.locomotion_direction;
  anim_ctx.yaw_degrees = p.yaw_degrees;
  anim_ctx.yaw_radians = basis.yaw_radians;
  anim_ctx.has_movement_target = locomotion_state.has_movement_target;
  anim_ctx.move_speed = locomotion_state.move_speed;
  anim_ctx.movement_target = locomotion_state.movement_target;
  anim_ctx.locomotion_velocity = locomotion_state.locomotion_velocity;
  anim_ctx.gait = locomotion_state.gait;
  anim_ctx.locomotion_cycle_time = locomotion_state.gait.cycle_time;
  anim_ctx.locomotion_phase = locomotion_state.gait.cycle_phase;
  anim_ctx.construction_role = construction.role;
  anim_ctx.construction_blend = construction.blend;
  anim_ctx.attack_phase = anim_ctx.inputs.combat_visual.attack_phase;
  anim_ctx.attack_emphasis = anim_ctx.inputs.combat_visual.attack_emphasis;
  anim_ctx.amplified_attack = false;
  anim_ctx.finisher_attack = anim_ctx.inputs.combat_visual.finisher_attack;

  apply_soldier_ambient_idle(sc, persistent, render_has_locomotion, anim_ctx);
  return anim_ctx;
}

struct SoldierGrounding {
  bool world_already_grounded{false};
  bool surface_height_valid{false};
  float surface_world_y{0.0F};
};

auto ground_soldier(const SoldierScope& sc,
                    const SoldierPlacement& p,
                    const SoldierBasis& basis,
                    const HumanoidAnimationContext& anim_ctx,
                    DrawContext& inst_ctx) -> SoldierGrounding {
  const DrawContext& ctx = *sc.unit.ctx;
  const auto& visual_spec = sc.unit.visual_spec;
  auto* cache = sc.cache;

  bool const prepare_shadow_grounding =
      RCP::pass_intent_from_ctx(inst_ctx) == RCP::RenderPassIntent::Shadow;
  SoldierGrounding grounding;
  grounding.world_already_grounded = ctx.skip_ground_offset || prepare_shadow_grounding;
  if (!ctx.skip_ground_offset && prepare_shadow_grounding) {
    auto const* grounding_asset =
        Render::Creature::Pipeline::CreatureAssetRegistry::instance().resolve(
            visual_spec);
    std::uint32_t const grounding_species =
        grounding_asset != nullptr ? grounding_asset->bpat_species_id
                                   : Render::Creature::Bpat::k_species_humanoid;
    auto const grounding_archetype =
        (visual_spec.archetype_id != Render::Creature::k_invalid_archetype)
            ? visual_spec.archetype_id
            : Render::Creature::ArchetypeRegistry::k_humanoid_base;
    HumanoidPose const pose{};
    float const grounded_contact_y = RCP::grounded_humanoid_contact_y(
        grounding_archetype, grounding_species, pose, anim_ctx);
    grounding.surface_world_y =
        RCP::ground_model_contact_to_surface(ctx.world_view.terrain_or_empty(),
                                             inst_ctx.model,
                                             grounded_contact_y,
                                             basis.combined_height_scale,
                                             p.entity_ground_offset);
    grounding.surface_height_valid = true;
  } else if (!grounding.world_already_grounded) {
    constexpr float k_ground_cache_epsilon = 1.0e-3F;
    SoldierGroundSample* ground_sample = nullptr;
    if (cache != nullptr && sc.slot < cache->ground_samples.size()) {
      ground_sample = &cache->ground_samples[sc.slot];
    }
    QVector3D const pre_ground_origin = RCP::model_world_origin(inst_ctx.model);
    if (ground_sample != nullptr && ground_sample->valid && !p.elevated &&
        std::abs(ground_sample->x - pre_ground_origin.x()) < k_ground_cache_epsilon &&
        std::abs(ground_sample->z - pre_ground_origin.z()) < k_ground_cache_epsilon) {
      RCP::set_model_world_y(inst_ctx.model, ground_sample->model_y);
      grounding.surface_world_y = ground_sample->surface_y;
    } else {
      grounding.surface_world_y =
          RCP::ground_model_contact_to_surface(ctx.world_view.terrain_or_empty(),
                                               inst_ctx.model,
                                               0.0F,
                                               basis.combined_height_scale,
                                               p.entity_ground_offset);
      if (ground_sample != nullptr) {
        ground_sample->x = pre_ground_origin.x();
        ground_sample->z = pre_ground_origin.z();
        ground_sample->model_y = RCP::model_world_origin(inst_ctx.model).y();
        ground_sample->surface_y = grounding.surface_world_y;
        ground_sample->valid = true;
      }
    }
    grounding.surface_height_valid = true;
  }
  return grounding;
}

void apply_soldier_wading(const SoldierScope& sc,
                          const DrawContext& inst_ctx,
                          HumanoidAnimationContext& anim_ctx) {
  if (sc.unit.is_mounted_spawn) {
    return;
  }
  auto const& wade_terrain = sc.unit.ctx->world_view.terrain_or_empty();
  if (!wade_terrain.has_fords()) {
    return;
  }
  QVector3D const feet = RCP::model_world_origin(inst_ctx.model);
  float const depth = wade_terrain.ford_water_depth_at(feet.x(), feet.z());
  float const amount =
      std::clamp((depth - k_wade_pose_start_depth) / k_wade_pose_full_span, 0.0F, 1.0F);
  anim_ctx.inputs.is_wading = amount > 0.0F;
  anim_ctx.inputs.wade_amount = amount * amount * (3.0F - (2.0F * amount));
}

void apply_dodge_roll(const Engine::Core::CreaturePresentationComponent& presentation,
                      const QVector3D& fallback_axis,
                      DrawContext& inst_ctx) {
  float const phase = std::clamp(presentation.dodge_phase, 0.0F, 1.0F);
  float const eased = phase * phase * (3.0F - 2.0F * phase);
  QVector3D const root = RCP::model_world_origin(inst_ctx.model);
  QVector3D const pivot = root + QVector3D(0.0F, 0.68F, 0.0F);
  QVector3D roll_axis = QVector3D::crossProduct(
      QVector3D(0.0F, 1.0F, 0.0F),
      QVector3D(presentation.dodge_dir_x, 0.0F, presentation.dodge_dir_z));
  if (roll_axis.lengthSquared() < 1.0e-4F) {
    roll_axis = fallback_axis;
  }
  QMatrix4x4 roll;
  roll.translate(pivot);
  roll.rotate(-360.0F * eased, roll_axis.normalized());
  roll.translate(-pivot);
  inst_ctx.model = roll * inst_ctx.model;

  QVector3D const rolled_root = RCP::model_world_origin(inst_ctx.model);
  QMatrix4x4 anchor_correction;
  anchor_correction.translate(
      root.x() - rolled_root.x(), 0.0F, root.z() - rolled_root.z());
  inst_ctx.model = anchor_correction * inst_ctx.model;
}

auto commander_jump_pitch_target(const SoldierScope& sc) -> float {
  const auto& commander_jump = sc.animation.commander_jump;
  const auto* creature_presentation = sc.unit.creature_presentation;
  float const air = std::sin(std::clamp(commander_jump.phase, 0.0F, 1.0F) *
                             std::numbers::pi_v<float>);
  auto const* action_definition =
      creature_presentation != nullptr &&
              creature_presentation->authored_action_id != 0U
          ? Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    creature_presentation->authored_action_id))
          : nullptr;
  bool const diving = action_definition != nullptr &&
                      action_definition->role ==
                          Game::Systems::CombatActions::CommanderActionRole::Dive;
  return (diving ? 34.0F : -10.0F) * air;
}

auto apply_commander_airborne_pose(const SoldierScope& sc,
                                   const SoldierBasis& basis,
                                   const SoldierPersistentState& persistent,
                                   DrawContext& inst_ctx) -> bool {
  const auto& commander_jump = sc.animation.commander_jump;
  const auto* creature_presentation = sc.unit.creature_presentation;

  if (commander_jump.height_offset > 0.0F) {
    RCP::set_model_world_y(inst_ctx.model,
                           RCP::model_world_origin(inst_ctx.model).y() +
                               commander_jump.height_offset);
  }
  bool const dodging =
      creature_presentation != nullptr && creature_presentation->dodge_active;
  if (dodging) {
    apply_dodge_roll(*creature_presentation, basis.right, inst_ctx);
  }
  float const jump_pitch_target =
      !dodging && commander_jump.active ? commander_jump_pitch_target(sc) : 0.0F;
  float jump_pitch = jump_pitch_target;
  if (persistent.animation != nullptr) {
    constexpr float k_jump_pitch_tau = 0.06F;
    auto& root_state = *persistent.animation;
    float const dt =
        root_state.jump_pitch_time >= 0.0F
            ? std::clamp(sc.anim.time - root_state.jump_pitch_time, 0.0F, 0.25F)
            : -1.0F;
    if (dt >= 0.0F) {
      float const follow = 1.0F - std::exp(-dt / k_jump_pitch_tau);
      jump_pitch = root_state.jump_pitch_degrees +
                   (jump_pitch_target - root_state.jump_pitch_degrees) * follow;
    }
    if (sc.formation.allow_animation_persistence) {
      root_state.jump_pitch_degrees = dodging ? 0.0F : jump_pitch;
      root_state.jump_pitch_time = sc.anim.time;
    }
  }
  if (!dodging && std::abs(jump_pitch) > 0.01F) {
    QVector3D const pivot = RCP::model_world_origin(inst_ctx.model);
    QMatrix4x4 tilt;
    tilt.translate(pivot);
    tilt.rotate(jump_pitch, basis.right);
    tilt.translate(-pivot);
    inst_ctx.model = tilt * inst_ctx.model;
  }
  return dodging;
}

void apply_casualty_settle(const SoldierScope& sc,
                           const SoldierPlacement& p,
                           float casualty_offset_y,
                           const AnimationInputs& a,
                           DrawContext& inst_ctx) {
  float const corpse_sink =
      RCP::corpse_sink_offset(RCP::CreatureKind::Humanoid, a.death_sink_progress);
  if (casualty_offset_y > 0.0F || corpse_sink < 0.0F) {
    RCP::set_model_world_y(inst_ctx.model,
                           RCP::model_world_origin(inst_ctx.model).y() +
                               casualty_offset_y + corpse_sink);
  }
  if (p.casualty_body && !sc.unit.ctx->skip_ground_offset) {

    float const fall = a.is_dead ? 1.0F : std::clamp(a.death_progress, 0.0F, 1.0F);
    Render::tilt_model_to_ground(inst_ctx.model,
                                 sc.unit.ctx->world_view.terrain_or_empty(),
                                 k_corpse_max_ground_tilt_degrees,
                                 fall * fall * (3.0F - 2.0F * fall));
  }
}

auto resolve_soldier_root_motion(const SoldierScope& sc,
                                 const SoldierPlacement& p,
                                 const SoldierBasis& basis,
                                 const SoldierReaction& reaction)
    -> Animation::CombatRootMotionSample {
  const AnimationInputs& a = reaction.anim;
  bool const combat_root_eligible =
      !p.casualty_body && !sc.unit.is_mounted_spawn && !a.has_authored_action_phase &&
      !a.has_authored_action_clip && !a.is_in_hold_mode && !a.is_constructing;
  if (!combat_root_eligible) {
    return {};
  }
  auto const* directive = sc.presented;
  float recoil_dir_x = a.hit_recoil_x;
  float recoil_dir_z = a.hit_recoil_z;
  if (std::abs(recoil_dir_x) < 1.0e-5F && std::abs(recoil_dir_z) < 1.0e-5F) {
    recoil_dir_x = -basis.forward.x();
    recoil_dir_z = -basis.forward.z();
  }
  return Animation::resolve_combat_root_motion({
      .attacking = a.combat_visual.active && a.is_attacking,
      .melee = a.combat_visual.is_melee,
      .mounted = sc.unit.is_mounted_spawn,
      .formation_member = directive != nullptr && !sc.unit.ctx->force_single_soldier &&
                          !directive->damage_carrier,
      .phase = a.combat_visual.phase,
      .attack_phase = a.combat_visual.attack_phase,
      .attack_family = a.combat_visual.attack_family,
      .swing_outcome =
          static_cast<Animation::MeleeSwingOutcome>(a.attack_exchange_outcome),
      .hit_reacting = a.is_hit_reacting || reaction.swing_recoil_active,
      .reaction = Animation::hit_reaction_form_from_kind(
          static_cast<std::uint8_t>(a.hit_reaction_kind)),
      .reaction_progress = a.hit_reaction_progress,
      .reaction_intensity = reaction.hit_reaction_base_intensity,
      .recoil_dir_x = recoil_dir_x,
      .recoil_dir_z = recoil_dir_z,
      .body_displaced_by_simulation =
          directive == nullptr && !sc.unit.has_shared_formation_layout,
      .simulation_owns_lunge = a.simulation_owns_root_motion,
      .seed = sc.layout.inst_seed,
  });
}

void apply_soldier_combat_root(const SoldierScope& sc,
                               const SoldierPlacement& p,
                               const SoldierBasis& basis,
                               const SoldierPersistentState& persistent,
                               const SoldierReaction& reaction,
                               DrawContext& inst_ctx) {
  const AnimationInputs& a = reaction.anim;
  float const time = sc.anim.time;
  bool const persist = sc.formation.allow_animation_persistence;
  Animation::CombatRootMotionSample root_motion =
      resolve_soldier_root_motion(sc, p, basis, reaction);

  if (persistent.animation != nullptr) {
    constexpr float k_lunge_resume_seconds = 0.4F;
    auto& root_state = *persistent.animation;
    bool const reacting = a.is_hit_reacting || reaction.swing_recoil_active;
    if (persist && reacting) {
      root_state.last_reaction_time = time;
    }
    if (!reacting && root_state.last_reaction_time >= 0.0F) {
      float const resume = std::clamp(
          (time - root_state.last_reaction_time) / k_lunge_resume_seconds, 0.0F, 1.0F);
      root_motion.forward_offset *= resume;
      root_motion.pitch_degrees *= resume;
      root_motion.roll_degrees *= resume;
    }
  }

  CombatRootSmoothingTarget root_target{};
  root_target.time = time;
  if (root_motion.active && !a.simulation_owns_root_motion &&
      !sc.unit.has_shared_formation_layout) {
    root_target.offset_x =
        root_motion.world_offset_x + basis.forward.x() * root_motion.forward_offset;
    root_target.offset_z =
        root_motion.world_offset_z + basis.forward.z() * root_motion.forward_offset;
  }
  if (root_motion.active) {
    root_target.pitch_degrees = root_motion.pitch_degrees;
    root_target.roll_degrees = root_motion.roll_degrees;
  }

  CombatRootSmoothingTarget root_applied = root_target;
  if (persistent.animation != nullptr) {
    if (persist) {
      root_applied = smooth_combat_root(persistent.animation->combat_root, root_target);
    } else {
      CombatRootSmoothingState scratch = persistent.animation->combat_root;
      root_applied = smooth_combat_root(scratch, root_target);
    }
  }
  bool const squashing = root_motion.active && root_motion.squash > 0.001F;
  bool const root_active = std::abs(root_applied.offset_x) > 1.0e-4F ||
                           std::abs(root_applied.offset_z) > 1.0e-4F ||
                           std::abs(root_applied.pitch_degrees) > 0.01F ||
                           std::abs(root_applied.roll_degrees) > 0.01F || squashing;
  if (!root_active) {
    return;
  }
  QVector3D const root_origin = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
  QMatrix4x4 root;
  root.translate(root_applied.offset_x, 0.0F, root_applied.offset_z);
  root.translate(root_origin);
  if (std::abs(root_applied.pitch_degrees) > 0.01F) {
    root.rotate(root_applied.pitch_degrees, basis.right);
  }
  if (std::abs(root_applied.roll_degrees) > 0.01F) {
    root.rotate(root_applied.roll_degrees, basis.forward);
  }
  if (squashing) {
    root.scale(1.0F, 1.0F - root_motion.squash, 1.0F);
  }
  root.translate(-root_origin);
  inst_ctx.model = root * inst_ctx.model;
}

auto select_soldier_animation(const SoldierScope& sc,
                              const DrawContext& inst_ctx,
                              const AnimationInputs& a,
                              bool render_has_locomotion,
                              bool world_already_grounded,
                              const RCP::CreatureLodDecision& lod_decision,
                              const HumanoidAnimationContext& anim_ctx)
    -> RCP::CreatureGraphOutput {
  const DrawContext& ctx = *sc.unit.ctx;
  auto* cache = sc.cache;
  std::uint32_t const inst_seed = sc.layout.inst_seed;

  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &inst_ctx;
  graph_inputs.anim = &a;
  graph_inputs.entity = ctx.entity;
  graph_inputs.unit = sc.unit.unit_comp;
  graph_inputs.transform = sc.unit.transform_comp;
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.instance_index = static_cast<std::uint16_t>(sc.idx);
  graph_output.spec = RCP::finalize_visible_humanoid_spec(
      sc.unit.visual_spec, a, render_has_locomotion);
  graph_output.seed = inst_seed;
  graph_output.world_already_grounded = world_already_grounded;

  bool const selection_steady = humanoid_selection_is_steady(anim_ctx);
  SoldierSelectionCache* selection_slot =
      (cache != nullptr && sc.slot < cache->selection_cache.size())
          ? &cache->selection_cache[sc.slot]
          : nullptr;
  bool const selection_refresh_due =
      ((sc.frame_index + inst_seed) % k_selection_refresh_period) == 0U;
  bool const reuse_cached_selection =
      selection_steady && !selection_refresh_due && selection_slot != nullptr &&
      selection_slot->valid &&
      selection_slot->archetype == graph_output.spec.archetype_id &&
      selection_slot->movement_state ==
          static_cast<std::uint8_t>(anim_ctx.inputs.movement_state);
  if (reuse_cached_selection) {
    auto selection = selection_slot->selection;
    selection.phase = RCP::humanoid_phase_for_state(anim_ctx, selection.state);
    graph_output.humanoid_selection = selection;
    return graph_output;
  }

  graph_output.humanoid_selection = RCP::resolve_humanoid_animation_selection(
      graph_output.spec, anim_ctx, graph_output.seed, &sc.unit.variant);
  if (selection_slot != nullptr) {
    auto const& resolved = *graph_output.humanoid_selection;
    bool const single_clip =
        !resolved.full_body_blend.active() && !resolved.upper_body_overlay.active();
    selection_slot->valid = selection_steady && single_clip;
    if (selection_slot->valid) {
      selection_slot->archetype = graph_output.spec.archetype_id;
      selection_slot->movement_state =
          static_cast<std::uint8_t>(anim_ctx.inputs.movement_state);
      selection_slot->selection = *graph_output.humanoid_selection;
    }
  }
  return graph_output;
}

void apply_soldier_transition_link(const SoldierScope& sc,
                                   const SoldierPersistentState& persistent,
                                   RCP::CreatureGraphOutput& graph_output,
                                   HumanoidAnimationContext& anim_ctx) {
  if (persistent.animation == nullptr) {
    return;
  }
  auto const transition = RCP::apply_humanoid_transition_continuity(
      *graph_output.humanoid_selection,
      persistent.animation->transition,
      {.time = sc.anim.time,
       .persist = sc.formation.allow_animation_persistence,
       .hit_reaction = anim_ctx.inputs.is_hit_reacting,
       .dying = anim_ctx.inputs.is_dying || anim_ctx.inputs.is_dead,
       .working =
           anim_ctx.inputs.is_constructing ||
           anim_ctx.construction_role != Animation::HumanoidConstructionRole::None});
  if (transition.link_weight > 0.0F) {
    anim_ctx.inputs.action_link_weight = transition.link_weight;
  }
}

void advance_soldier_overlay(const SoldierScope& sc,
                             const SoldierPersistentState& persistent,
                             bool dodging,
                             const HumanoidAnimationContext& anim_ctx,
                             RCP::CreatureGraphOutput& graph_output) {
  if (persistent.animation == nullptr) {
    return;
  }
  auto& overlay = graph_output.humanoid_selection->upper_body_overlay;
  Render::Creature::PlaybackLayerRequest target{};
  if (overlay.active() && overlay.clip_id.has_value()) {
    target = {.archetype = overlay.archetype,
              .state = overlay.state,
              .phase = overlay.phase,
              .weight = overlay.weight,
              .clip_variant = overlay.clip_variant,
              .clip_id = *overlay.clip_id,
              .mode = overlay.mode};
  }
  auto const& in = anim_ctx.inputs;
  bool const combat_pose = in.is_attacking || in.is_guarding || in.is_hit_reacting;
  bool const owns_full_body =
      in.is_dying || in.is_dead || in.is_hit_reacting || in.is_attacking ||
      in.is_guarding || in.is_exiting_guard || in.is_in_hold_mode ||
      in.is_defensive_layout_locked || dodging || sc.animation.commander_jump.active;
  auto const shown = RCP::advance_humanoid_overlay(
      target,
      persistent.animation->overlay,
      {.time = sc.anim.time,
       .persist = sc.formation.allow_animation_persistence,
       .combat_pose = combat_pose,
       .full_body_takeover = owns_full_body && !target.active()});
  overlay = {};
  if (shown.active()) {
    overlay.archetype = shown.archetype;
    overlay.state = shown.state;
    overlay.phase = shown.phase;
    overlay.clip_variant = shown.clip_variant;
    overlay.clip_id = shown.clip_id;
    overlay.mode = shown.mode;
    overlay.weight = shown.weight;
  }
}

struct SoldierShadowInputs {
  const RCP::CreatureGraphOutput* graph{nullptr};
  const AnimationInputs* anim{nullptr};
  QVector3D world_pos{};
  HumanoidLOD lod{HumanoidLOD::Full};
  float camera_distance{0.0F};
  float yaw_degrees{0.0F};
  SoldierGrounding grounding{};
  float lens_gap_visibility{1.0F};
};

void append_soldier_shadow(const SoldierScope& sc,
                           const DrawContext& inst_ctx,
                           const SoldierShadowInputs& in,
                           HumanoidPreparation& out) {
  const AnimationInputs& a = *in.anim;
  RCP::HumanoidShadowStateInputs shadow_inputs{};
  shadow_inputs.ctx = &inst_ctx;
  shadow_inputs.graph = in.graph;
  shadow_inputs.unit = sc.unit.unit_comp;
  shadow_inputs.soldier_world_pos = in.world_pos;
  shadow_inputs.lod = in.lod;
  shadow_inputs.camera_distance = in.camera_distance;
  shadow_inputs.mounted = sc.unit.is_mounted_spawn;
  shadow_inputs.facing_yaw_degrees = in.yaw_degrees;
  shadow_inputs.intensity_scale = ((a.is_dying || a.is_dead) ? 0.45F : 1.0F) *
                                  RCP::corpse_shadow_scale(a.death_sink_progress);
  shadow_inputs.surface_world_y = in.grounding.surface_world_y;
  shadow_inputs.surface_height_valid = in.grounding.surface_height_valid;
  const auto shadow_state = RCP::prepare_humanoid_shadow_state(shadow_inputs);
  if (!shadow_state.enabled) {
    return;
  }
  if (out.shadow_batch.empty()) {
    out.shadow_batch.init(
        shadow_state.shader, shadow_state.mesh, shadow_state.light_dir);
  }
  out.shadow_batch.add(shadow_state.model,
                       shadow_state.alpha * in.lens_gap_visibility,
                       shadow_state.pass);
}

void append_soldier_bodies(const SoldierScope& sc,
                           const DrawContext& inst_ctx,
                           const RCP::CreatureGraphOutput& graph_output,
                           const HumanoidAnimationContext& anim_ctx,
                           HumanoidLOD lod,
                           float lens_gap_visibility,
                           HumanoidPreparation& out) {
  const auto& s = sc.unit;
  const DrawContext& ctx = *s.ctx;
  const HumanoidRendererBase& owner = *s.owner;
  const HumanoidVariant& variant = s.variant;
  auto& stats = *sc.animation.stats;
  HumanoidPose const pose{};
  auto const append_companions = [&]() {
    owner.append_companion_preparation(
        inst_ctx,
        variant,
        pose,
        anim_ctx,
        sc.layout.inst_seed,
        graph_output.lod,
        graph_output.humanoid_selection ? &*graph_output.humanoid_selection : nullptr,
        out);
  };

  const std::size_t first_body_request = out.bodies.requests().size();
  switch (lod) {
  case HumanoidLOD::Full:
    ++stats.lod_full;
    if (!s.is_mounted_spawn) {
      out.bodies.add_humanoid(graph_output, pose, variant, anim_ctx);
    }
    append_companions();
    break;

  case HumanoidLOD::Minimal:
    ++stats.lod_minimal;
    if (s.is_mounted_spawn && !ctx.allow_template_cache &&
        ctx.suppress_animation_state_persistence) {
      append_companions();
      break;
    }
    out.bodies.add_humanoid(graph_output, pose, variant, anim_ctx);
    break;

  case HumanoidLOD::Culled:
    break;
  }
  out.bodies.fade_requests_from(first_body_request, lens_gap_visibility);
}

void append_prepared_soldier(const HumanoidUnitSnapshot& s,
                             const HumanoidFormationRuntime& f,
                             HumanoidUnitAnimationRuntime& u,
                             const AnimationInputs& anim,
                             std::uint32_t frame_index,
                             HumanoidPreparation& out,
                             int idx,
                             const AnimationInputs& soldier_anim,
                             const CasualtyPose& casualty) {
  using namespace Render::GL;

  SoldierScope const sc = make_soldier_scope(s, f, u, anim, frame_index, idx);
  const DrawContext& ctx = *s.ctx;
  auto& stats = *u.stats;
  bool const record_animation_diagnostics = u.record_animation_diagnostics;

  SoldierReaction reaction = resolve_soldier_reaction(sc, soldier_anim);
  AnimationInputs& soldier_render_anim = reaction.anim;
  apply_guard_shield_pose(sc, soldier_render_anim);

  SoldierPlacement const placement = place_soldier(sc, casualty, soldier_render_anim);

  SoldierVisibility const visibility = evaluate_soldier_visibility(sc, placement);
  if (visibility.culled()) {
    count_soldier_cull(stats, visibility);
    if (record_animation_diagnostics) {
      SoldierDebugRecord record;
      record.attack_phase = soldier_render_anim.combat_visual.attack_phase;
      record.animation_state =
          Render::Creature::resolve_pose(soldier_render_anim).animation_state;
      record.lod = HumanoidLOD::Culled;
      record.cull_reason = visibility.cull_reason();
      record.root.position = placement.world_pos;
      record.root.yaw_degrees = placement.yaw_degrees;
      record_soldier_debug(sc,
                           soldier_render_anim,
                           {.swing_recoil_active = reaction.swing_recoil_active,
                            .last_swing_is_melee = soldier_render_anim.is_melee},
                           record);
    }
    return;
  }

  float const hit_reaction_tilt_degrees =
      record_animation_diagnostics
          ? Animation::resolve_humanoid_hit_reaction_transform(
                {
                    .active = soldier_render_anim.is_hit_reacting ||
                              reaction.swing_recoil_active,
                    .intensity = soldier_render_anim.hit_reaction_intensity,
                    .recoil_x = soldier_render_anim.hit_recoil_x,
                    .recoil_z = soldier_render_anim.hit_recoil_z,
                    .inst_seed = sc.layout.inst_seed,
                })
                .tilt_degrees
          : Animation::HumanoidHitReactionTransformSample{}.tilt_degrees;

  DrawContext& inst_ctx = u.inst_ctx;
  inst_ctx.model = placement.model;
  SoldierBasis const basis =
      resolve_soldier_basis(sc, placement, soldier_render_anim, inst_ctx);

  SoldierPersistentState const persistent = resolve_soldier_persistent_state(sc);
  bool const last_swing_is_melee = resolve_soldier_combat(sc, persistent, reaction);
  bool const render_has_locomotion =
      Render::Creature::is_moving_animation(soldier_render_anim.movement_state);
  HumanoidAnimationContext anim_ctx = build_soldier_animation_context(
      sc, placement, basis, persistent, render_has_locomotion, soldier_render_anim);

  SoldierGrounding const grounding =
      ground_soldier(sc, placement, basis, anim_ctx, inst_ctx);
  apply_soldier_wading(sc, inst_ctx, anim_ctx);
  bool const dodging = apply_commander_airborne_pose(sc, basis, persistent, inst_ctx);
  apply_casualty_settle(
      sc, placement, casualty.offset_y, soldier_render_anim, inst_ctx);
  apply_soldier_combat_root(sc, placement, basis, persistent, reaction, inst_ctx);
  anim_ctx.instance_position = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

  QVector3D const soldier_world_pos = anim_ctx.instance_position;
  SoldierDebugRoot debug_root;
  if (record_animation_diagnostics) {
    debug_root = measure_debug_root(inst_ctx.model,
                                    soldier_world_pos,
                                    placement.yaw_degrees,
                                    hit_reaction_tilt_degrees);
  }

  RCP::HumanoidLodStateInputs lod_inputs{};
  lod_inputs.ctx = &ctx;
  lod_inputs.soldier_world_pos = soldier_world_pos;
  lod_inputs.config = f.lod_config;
  const auto lod_state = RCP::resolve_humanoid_lod_state(lod_inputs);
  const auto& lod_decision = lod_state.decision;
  if (lod_decision.culled) {
    ++stats.soldiers_skipped_lod;
    if (record_animation_diagnostics) {
      SoldierDebugRecord record;
      record.attack_phase = anim_ctx.attack_phase;
      record.animation_state =
          Render::Creature::resolve_pose(anim_ctx.inputs).animation_state;
      record.lod = static_cast<HumanoidLOD>(lod_decision.lod);
      record.cull_reason = Render::Profiling::SoldierCullReason::Distance;
      record.root = debug_root;
      record.gait = debug_gait_of(anim_ctx);
      record_soldier_debug(sc,
                           anim_ctx.inputs,
                           {.swing_recoil_active = reaction.swing_recoil_active,
                            .last_swing_is_melee = last_swing_is_melee},
                           record);
    }
    return;
  }
  auto const soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

  anim_ctx.idle_breath_phase = RCP::humanoid_idle_breath_phase_for_lod(
      anim.time, sc.layout.inst_seed, soldier_lod, ctx.prewarming_via_runtime_path);

  ++stats.soldiers_rendered;

  inst_ctx.soldier_index = static_cast<std::uint16_t>(idx);
  auto graph_output = select_soldier_animation(sc,
                                               inst_ctx,
                                               soldier_render_anim,
                                               render_has_locomotion,
                                               grounding.world_already_grounded,
                                               lod_decision,
                                               anim_ctx);
  apply_soldier_transition_link(sc, persistent, graph_output, anim_ctx);
  advance_soldier_overlay(sc, persistent, dodging, anim_ctx, graph_output);

  if (sc.cache != nullptr && sc.slot < sc.cache->visibility_frames.size()) {
    sc.cache->visibility_frames[sc.slot] = frame_index + 1U;
  }
  if (record_animation_diagnostics) {
    bool const transient_recovery_override =
        (soldier_render_anim.is_attacking != anim_ctx.inputs.is_attacking) ||
        (soldier_render_anim.combat_phase != anim_ctx.inputs.combat_phase) ||
        (std::abs(soldier_render_anim.combat_phase_progress -
                  anim_ctx.inputs.combat_phase_progress) > 1.0e-4F);
    SoldierDebugRecord record;
    record.attack_phase = anim_ctx.attack_phase;
    record.animation_state = graph_output.humanoid_selection->state;
    record.lod = soldier_lod;
    record.cull_reason = Render::Profiling::SoldierCullReason::None;
    record.transient_recovery_override = transient_recovery_override;
    record.root = debug_root;
    record.gait = debug_gait_of(anim_ctx);
    record.animation_clip = graph_output.humanoid_selection->clip_id;
    record_soldier_debug(
        sc,
        anim_ctx.inputs,
        {.swing_recoil_active = reaction.swing_recoil_active,
         .last_swing_is_melee = last_swing_is_melee,
         .combat_ready_stance = graph_output.humanoid_selection->clip_id ==
                                Animation::k_humanoid_combat_ready_clip},
        record);
  }

  append_soldier_shadow(sc,
                        inst_ctx,
                        {.graph = &graph_output,
                         .anim = &soldier_render_anim,
                         .world_pos = soldier_world_pos,
                         .lod = soldier_lod,
                         .camera_distance = lod_state.camera_distance,
                         .yaw_degrees = placement.yaw_degrees,
                         .grounding = grounding,
                         .lens_gap_visibility = visibility.lens_gap_visibility},
                        out);
  append_soldier_bodies(sc,
                        inst_ctx,
                        graph_output,
                        anim_ctx,
                        soldier_lod,
                        visibility.lens_gap_visibility,
                        out);
}

} // namespace

void prepare_humanoid_instances(const HumanoidRendererBase& owner,
                                const DrawContext& ctx,
                                const AnimationInputs& anim,
                                HumanoidRuntimeContext& runtime,
                                HumanoidPreparation& out) {
  using namespace Render::GL;

  const std::uint32_t frame_index = runtime.frame_index;
  const HumanoidUnitSnapshot snapshot = snapshot_humanoid_unit(owner, ctx, anim);
  const HumanoidFormationRuntime formation_runtime =
      prepare_formation_runtime(snapshot, anim, frame_index);
  HumanoidUnitAnimationRuntime unit_animation =
      resolve_unit_animation_runtime(snapshot, formation_runtime, anim, runtime);

  const int visible_count = snapshot.visible_count;
  const std::size_t active_casualty_count = snapshot.active_casualty_count;
  auto& stats = runtime.stats;
  stats.soldiers_total +=
      visible_count + static_cast<std::uint32_t>(active_casualty_count);

  out.bodies.reserve(out.bodies.size() + static_cast<std::size_t>(visible_count) +
                     active_casualty_count);

  const int total_layout_count = snapshot.total_layout_count;
  auto& live_slot_indices = *snapshot.live_slot_indices;
  auto& soldier_layouts = *formation_runtime.soldier_layout_storage;
  auto* casualties_comp = snapshot.casualties_comp;

  auto append_soldier = [&](int idx,
                            const AnimationInputs& soldier_anim,
                            const CasualtyPose& casualty = {}) {
    append_prepared_soldier(snapshot,
                            formation_runtime,
                            unit_animation,
                            anim,
                            frame_index,
                            out,
                            idx,
                            soldier_anim,
                            casualty);
  };

  for (int const idx : live_slot_indices) {
    append_soldier(idx, anim);
  }

  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    auto const& entries = casualties_comp->entries;
    auto* anchor_cache = formation_runtime.layout_cache_comp;
    if (anchor_cache != nullptr) {
      std::erase_if(anchor_cache->casualty_anchors, [&entries](auto const& anchor) {
        return std::none_of(
            entries.begin(), entries.end(), [&anchor](auto const& entry) {
              return entry.slot_index == anchor.slot_index;
            });
      });
    }
    for (const auto& entry : entries) {
      if (entry.slot_index >= total_layout_count ||
          std::find(live_slot_indices.begin(),
                    live_slot_indices.end(),
                    static_cast<int>(entry.slot_index)) != live_slot_indices.end()) {
        continue;
      }

      AnimationInputs casualty_anim{};
      casualty_anim.time = anim.time;
      casualty_anim.death_variant = entry.sequence_variant;
      if (entry.state == Engine::Core::DeathSequenceState::Dying) {
        casualty_anim.is_dying = true;
        casualty_anim.death_progress =
            (entry.state_duration > 0.0F)
                ? std::clamp(entry.state_time / entry.state_duration, 0.0F, 1.0F)
                : 1.0F;
      } else {
        casualty_anim.is_dead = true;
        casualty_anim.death_progress = 1.0F;
        casualty_anim.death_sink_progress = Engine::Core::death_sink_progress(entry);
      }
      float launch_velocity_x = entry.launch_velocity_x;
      float launch_velocity_z = entry.launch_velocity_z;
      auto const& casualty_layout =
          soldier_layouts[static_cast<std::size_t>(entry.slot_index)];
      float rest_x = entry.has_local_anchor ? entry.local_x : casualty_layout.offset_x;
      float rest_z = entry.has_local_anchor ? entry.local_z : casualty_layout.offset_z;
      float rest_yaw =
          entry.has_local_anchor ? entry.local_yaw : casualty_layout.yaw_offset;
      const QMatrix4x4* casualty_base = nullptr;
      float casualty_root_yaw = 0.0F;
      if (anchor_cache != nullptr) {
        auto anchor = std::find_if(anchor_cache->casualty_anchors.begin(),
                                   anchor_cache->casualty_anchors.end(),
                                   [&entry](auto const& candidate) {
                                     return candidate.slot_index == entry.slot_index;
                                   });
        if (anchor == anchor_cache->casualty_anchors.end()) {
          anchor_cache->casualty_anchors.push_back({entry.slot_index,
                                                    unit_animation.unit_base,
                                                    unit_animation.root_yaw,
                                                    rest_x,
                                                    rest_z,
                                                    rest_yaw,
                                                    launch_velocity_x,
                                                    launch_velocity_z});
          anchor = std::prev(anchor_cache->casualty_anchors.end());
        }
        casualty_base = &anchor->frame;
        casualty_root_yaw = anchor->root_yaw;
        rest_x = anchor->rest_x;
        rest_z = anchor->rest_z;
        rest_yaw = anchor->rest_yaw;
        launch_velocity_x = anchor->launch_velocity_x;
        launch_velocity_z = anchor->launch_velocity_z;
      }
      const CasualtyLaunch launch =
          resolve_casualty_launch(entry, launch_velocity_x, launch_velocity_z);
      append_soldier(static_cast<int>(entry.slot_index),
                     casualty_anim,
                     {.offset_x = rest_x - casualty_layout.offset_x + launch.x,
                      .offset_y = launch.y,
                      .offset_z = rest_z - casualty_layout.offset_z + launch.z,
                      .pitch = launch.pitch,
                      .roll = launch.roll,
                      .yaw = rest_yaw - casualty_layout.yaw_offset,
                      .base = casualty_base,
                      .root_yaw = casualty_root_yaw});
    }
  }
}

} // namespace Render::Humanoid
