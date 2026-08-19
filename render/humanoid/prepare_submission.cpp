#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "animation/combat_root_motion_manifest.h"
#include "prepare_internal.h"
#include "render/submission_visibility.h"

namespace Render::Humanoid {

namespace {

struct CasualtyLaunch {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float pitch{0.0F};
  float roll{0.0F};
};

auto resolve_casualty_launch(
    const Engine::Core::SoldierCasualtyAnimationComponent::Entry& entry)
    -> CasualtyLaunch {
  if (!entry.launched) {
    return {};
  }

  constexpr float k_flight_seconds = 1.45F;
  constexpr float k_half_gravity = 4.9F;

  float const casualty_age = entry.state == Engine::Core::DeathSequenceState::Dying
                                 ? entry.state_time
                                 : entry.state_duration + entry.state_time;
  float const flight_time = std::min(casualty_age, k_flight_seconds);

  return {entry.launch_velocity_x * flight_time,
          std::max(0.0F,
                   entry.launch_velocity_y * flight_time -
                       k_half_gravity * flight_time * flight_time),
          entry.launch_velocity_z * flight_time,
          entry.launch_pitch_speed * flight_time,
          entry.launch_roll_speed * flight_time};
}

constexpr float k_soldier_cull_enter_band = 1.5F;
constexpr float k_soldier_cull_keep_band = 5.0F;
constexpr std::uint32_t k_soldier_visibility_memory_frames = 12U;
constexpr std::uint32_t k_selection_refresh_period = 8U;

auto humanoid_selection_is_steady(
    const Render::GL::HumanoidAnimationContext& anim_ctx) noexcept -> bool {
  auto const& in = anim_ctx.inputs;
  if (in.is_attacking || in.is_casting || in.is_mounted || in.has_showcase_clip ||
      in.has_authored_action_clip || in.is_in_hold_mode || in.is_exiting_hold ||
      in.is_guarding || in.is_exiting_guard || in.is_hit_reacting || in.is_healing ||
      in.is_routing || in.is_constructing || in.is_dying || in.is_dead ||
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

} // namespace

void prepare_humanoid_instances(const HumanoidRendererBase& owner,
                                const DrawContext& ctx,
                                const AnimationInputs& anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation& out) {
  using namespace Render::GL;

  if (ctx.entity != nullptr && ctx.world == nullptr) {

    Engine::Core::publish_creature_presentation(ctx.entity, nullptr);
  }

  auto& profile = Render::Profiling::global_profile();

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
    is_mounted_spawn = is_mounted_spawn || st == SpawnType::MountedKnight ||
                       st == SpawnType::HorseArcher || st == SpawnType::HorseSpearman;
  }

  int const total_layout_count =
      std::min(formation.individuals_per_unit, effective_rows * cols);
  bool const has_entity_death = anim.is_dying || anim.is_dead;
  auto* casualties_comp =
      (ctx.entity != nullptr)
          ? ctx.entity->get_component<Engine::Core::SoldierCasualtyAnimationComponent>()
          : nullptr;
  thread_local std::vector<int> live_slot_indices;
  live_slot_indices.clear();
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

  if (!owner.m_proportion_scale_cached) {
    owner.m_cached_proportion_scale = owner.get_proportion_scaling();
    owner.m_proportion_scale_cached = true;
  }
  const QVector3D prop_scale = owner.m_cached_proportion_scale;
  const float height_scale = prop_scale.y();
  const bool needs_height_scaling = std::abs(height_scale - 1.0F) > 0.001F;

  const QMatrix4x4 k_identity_matrix;

  Game::Formation::UnitLayoutId unit_layout = ctx.world_view.unit_layouts()->resolve(
      Game::Formation::k_neutral_doctrine, "close_order_infantry");
  float formed_ratio = 1.0F;
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
      formed_ratio = Game::Formation::UnitLayoutStateSystem::formed_ratio(*ctx.entity);
      if (layout_state->layout_id != Game::Formation::k_invalid_layout) {
        unit_layout = layout_state->layout_id;
      }
    }
  }

  s_render_stats.soldiers_total +=
      visible_count + static_cast<std::uint32_t>(active_casualty_count);

  out.bodies.reserve(out.bodies.size() + static_cast<std::size_t>(visible_count) +
                     active_casualty_count);

  namespace RCP = Render::Creature::Pipeline;
  const auto lod_config = RCP::humanoid_lod_config_from_settings();
  std::vector<SoldierLayout> transient_soldier_layouts;
  std::vector<SoldierLayout>* soldier_layout_storage = &transient_soldier_layouts;
  HumanoidLayoutCacheComponent* layout_cache_comp =
      (ctx.entity != nullptr)
          ? Engine::Core::get_or_add_component<HumanoidLayoutCacheComponent>(ctx.entity)
          : nullptr;
  bool const allow_animation_persistence = should_persist_animation_state(ctx);
  bool preserve_soldier_state_prefix = false;
  bool loaded_cached_layouts = false;
  {
    Render::Profiling::AccumulatorScope const layout_scope(
        ctx.template_prewarm ? nullptr : &profile.soldier_layout_generation_us);

    if (layout_cache_comp != nullptr && layout_cache_comp->valid) {
      auto& entry = *layout_cache_comp;
      preserve_soldier_state_prefix =
          entry.seed == seed && entry.rows == rows && entry.cols == cols &&
          entry.layout_version == k_humanoid_layout_cache_version &&
          entry.formation.individuals_per_unit == formation.individuals_per_unit &&
          entry.formation.max_per_row == formation.max_per_row &&
          entry.formation.spacing == formation.spacing &&
          entry.unit_layout == unit_layout;
      bool const matches =
          preserve_soldier_state_prefix &&
          entry.soldiers.size() == static_cast<std::size_t>(total_layout_count);
      bool const cache_valid = !matches
                                   ? false
                                   : ((anim.is_attacking && anim.is_melee)
                                          ? (entry.frame_number == frame_index)
                                          : (frame_index - entry.frame_number <=
                                             ::Render::GL::k_layout_cache_max_age));
      if (cache_valid) {
        soldier_layout_storage = &entry.soldiers;
        entry.frame_number = frame_index;
        loaded_cached_layouts = true;
      }
    }

    if (!loaded_cached_layouts) {
      soldier_layout_storage = layout_cache_comp != nullptr
                                   ? &layout_cache_comp->soldiers
                                   : &transient_soldier_layouts;
      auto& generated_layouts = *soldier_layout_storage;
      generated_layouts.clear();
      generated_layouts.reserve(static_cast<std::size_t>(total_layout_count));
      for (int idx = 0; idx < total_layout_count; ++idx) {
        auto const slot = Game::Formation::rank_slot_for(idx, total_layout_count, cols);
        SoldierLayoutInputs inputs{};
        inputs.idx = idx;
        inputs.row = ctx.force_single_soldier ? rows - 1 : slot.row;
        inputs.col = slot.col;
        inputs.rows = rows;
        inputs.cols = cols;
        inputs.count = total_layout_count;
        inputs.formation_spacing = formation.spacing;
        inputs.seed = seed;
        inputs.force_single_soldier = ctx.force_single_soldier;
        inputs.melee_attack = anim.is_attacking && anim.is_melee;
        inputs.animation_time = anim.time;
        inputs.unit_layout = unit_layout;
        inputs.formed_ratio = formed_ratio;
        inputs.soldier_offsets = ctx.world_view.soldier_offsets();
        generated_layouts.push_back(build_soldier_layout(inputs));
      }

      if (layout_cache_comp != nullptr) {
        layout_cache_comp->formation = formation;
        layout_cache_comp->unit_layout = unit_layout;
        layout_cache_comp->rows = rows;
        layout_cache_comp->cols = cols;
        layout_cache_comp->layout_version = k_humanoid_layout_cache_version;
        layout_cache_comp->seed = seed;
        layout_cache_comp->frame_number = frame_index;
        layout_cache_comp->valid = true;
      }
    }
  }

  auto& soldier_layouts = *soldier_layout_storage;

  if (!anim.is_constructing) {
    if (has_shared_formation_layout &&
        formation_presentation->soldiers.size() == soldier_layouts.size()) {
      for (std::size_t index = 0; index < soldier_layouts.size(); ++index) {
        auto const& shared_slot = formation_presentation->soldiers[index];
        auto& render_slot = soldier_layouts[index];
        render_slot.offset_x = shared_slot.local_x;
        render_slot.offset_z = shared_slot.local_z;
        render_slot.yaw_offset = shared_slot.local_yaw;
        render_slot.row_index = static_cast<std::uint8_t>(shared_slot.row);
        render_slot.col_index = static_cast<std::uint8_t>(shared_slot.col);
      }
    } else if (ctx.entity != nullptr && !ctx.force_single_soldier) {

      auto const layout = Game::Systems::FormationCombat::resolve_layout(*ctx.entity);
      for (auto const& slot : layout.occupied_slots) {
        if (slot.index >= soldier_layouts.size()) {
          continue;
        }
        auto& render_slot = soldier_layouts[slot.index];
        render_slot.offset_x = slot.local_x;
        render_slot.offset_z = slot.local_z;
        render_slot.yaw_offset = slot.local_yaw;
        render_slot.row_index = static_cast<std::uint8_t>(slot.row);
        render_slot.col_index = static_cast<std::uint8_t>(slot.col);
      }
    }
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
    if (!preserve_soldier_state_prefix) {
      for (auto& state : animation_states) {
        reset_humanoid_locomotion_state(state);
      }
      for (auto& lane_state : combat_lanes) {
        lane_state = {};
      }
      std::fill(visibility_frames.begin(), visibility_frames.end(), 0U);
    } else if (state_count_changed) {
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
  auto record_soldier_debug = [&](int idx,
                                  const AnimationInputs&,
                                  const AnimationInputs& resolved_anim,
                                  float attack_phase,
                                  Render::Creature::AnimationStateId animation_state,
                                  HumanoidLOD lod,
                                  Render::Profiling::SoldierCullReason cull_reason,
                                  bool transient_recovery_override,
                                  const QVector3D& root_position,
                                  float root_yaw_degrees,
                                  float root_up_y,
                                  float root_scale_y,
                                  float root_tilt_degrees,
                                  float hit_reaction_tilt_degrees) {
    if (!record_animation_diagnostics || ctx.template_prewarm ||
        ctx.entity == nullptr) {
      return;
    }

    Render::Profiling::SoldierAnimationDebugSample sample{};
    sample.soldier_index = idx;
    sample.sample_time = resolved_anim.time;
    sample.combat_phase = resolved_anim.combat_phase;
    sample.combat_phase_progress = resolved_anim.combat_phase_progress;
    sample.attack_phase = attack_phase;
    sample.attack_variant = resolved_anim.attack_variant;
    sample.is_attacking = resolved_anim.is_attacking;
    sample.is_hit_reacting = resolved_anim.is_hit_reacting;
    sample.is_in_melee_lock = resolved_anim.is_in_melee_lock;
    sample.transient_recovery_override = transient_recovery_override;
    sample.locomotion_state = resolved_anim.movement_state;
    sample.animation_state = animation_state;
    sample.lod = static_cast<std::uint8_t>(lod);
    sample.cull_reason = cull_reason;
    sample.root_position = root_position;
    sample.root_yaw_degrees = root_yaw_degrees;
    sample.root_up_y = root_up_y;
    sample.root_scale_y = root_scale_y;
    sample.root_tilt_degrees = root_tilt_degrees;
    sample.hit_reaction_tilt_degrees = hit_reaction_tilt_degrees;
    animation_diagnostics.record_soldier_sample(ctx.entity->get_id(), sample);
  };

  bool const formation_fight_active = has_shared_formation_layout &&
                                      formation_presentation->melee_ordered &&
                                      formation_presentation->target_alive;

  constexpr float k_formation_fog_radius = 6.0F;
  DrawContext inst_ctx = ctx;
  QMatrix4x4 unit_base = k_identity_matrix;
  if (transform_comp != nullptr) {
    unit_base.translate(transform_comp->position.x,
                        transform_comp->position.y,
                        transform_comp->position.z);
    unit_base.rotate(transform_comp->rotation.y, 0.0F, 1.0F, 0.0F);
  }
  const auto locomotion_override =
      Animation::resolve_humanoid_locomotion_action_override({
          .commander_jump_active = commander_jump.active,
      });

  float turn_smoothing_dt = 0.0F;
  bool turn_smoothing_active = false;
  float turn_smoothing_cap = 2.5F;
  bool turn_smoothing_travel_yaw = true;
  if (layout_cache_comp != nullptr && allow_animation_persistence &&
      transform_comp != nullptr && !ctx.force_single_soldier && !has_entity_death &&
      total_layout_count > 1) {
    turn_smoothing_active = true;
    auto& turn_cache = *layout_cache_comp;
    if (turn_cache.turn_states.size() != static_cast<std::size_t>(total_layout_count)) {
      turn_cache.turn_states.assign(static_cast<std::size_t>(total_layout_count),
                                    SoldierTurnSmoothingState{});
    }
    if (turn_cache.turn_time_valid) {
      constexpr float k_turn_smoothing_max_dt = 0.1F;
      turn_smoothing_dt =
          std::clamp(anim.time - turn_cache.turn_time, 0.0F, k_turn_smoothing_max_dt);
    }
    turn_cache.turn_time = anim.time;
    turn_cache.turn_time_valid = true;
    float const unit_speed = unit_comp != nullptr ? unit_comp->speed : 2.0F;
    turn_smoothing_cap = std::max(1.6F, unit_speed * 1.25F);
    bool const combat_active =
        anim.is_attacking || formation_fight_active || anim.is_in_melee_lock;
    if (combat_active) {
      turn_smoothing_cap *= 2.0F;
      turn_smoothing_travel_yaw = false;
    }
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

  auto append_prepared_soldier = [&](int idx,
                                     const AnimationInputs& soldier_anim,
                                     float casualty_offset_x = 0.0F,
                                     float casualty_offset_y = 0.0F,
                                     float casualty_offset_z = 0.0F,
                                     float casualty_pitch = 0.0F,
                                     float casualty_roll = 0.0F,
                                     float casualty_yaw = 0.0F) {
    SoldierLayout const& layout = soldier_layouts[static_cast<std::size_t>(idx)];
    AnimationInputs soldier_render_anim = soldier_anim;
    if (creature_presentation != nullptr &&
        !creature_presentation->allow_full_body_hit_reaction &&
        soldier_render_anim.is_hit_reacting) {

      soldier_render_anim.is_hit_reacting = false;
      soldier_render_anim.hit_reaction_intensity = 0.0F;
      soldier_render_anim.hit_recoil_x = 0.0F;
      soldier_render_anim.hit_recoil_z = 0.0F;
    }
    float hit_reaction_base_intensity =
        soldier_render_anim.is_hit_reacting
            ? soldier_render_anim.hit_reaction_intensity /
                  std::max(0.05F, 1.0F - soldier_render_anim.hit_reaction_progress)
            : 0.0F;
    if (formation_hit != nullptr && formation_hit->remaining > 0.0F &&
        formation_hit->soldier_slot == static_cast<std::uint16_t>(idx)) {
      soldier_render_anim.is_hit_reacting = true;
      soldier_render_anim.hit_reaction_intensity = formation_hit->intensity;
      hit_reaction_base_intensity = formation_hit->intensity;
      soldier_render_anim.hit_reaction_kind = formation_hit->reaction_kind;
      float const hit_duration = formation_hit->duration > 0.0F
                                     ? formation_hit->duration
                                     : std::max(formation_hit->remaining, 0.28F);
      soldier_render_anim.hit_reaction_progress =
          std::clamp(1.0F - formation_hit->remaining / hit_duration, 0.0F, 1.0F);
      soldier_render_anim.hit_recoil_x = formation_hit->hit_direction_x;
      soldier_render_anim.hit_recoil_z = formation_hit->hit_direction_z;
      if (std::abs(soldier_render_anim.hit_recoil_x) < 1.0e-4F &&
          std::abs(soldier_render_anim.hit_recoil_z) < 1.0e-4F &&
          ctx.entity != nullptr) {
        if (auto const* feedback =
                ctx.entity->get_component<Engine::Core::HitFeedbackComponent>()) {
          soldier_render_anim.hit_recoil_x = feedback->hit_direction_x;
          soldier_render_anim.hit_recoil_z = feedback->hit_direction_z;
        }
      }
    }

    bool const authored_swing_owns_body =
        soldier_render_anim.has_authored_action_phase &&
        soldier_render_anim.is_attacking;
    bool swing_recoil_active =
        (authored_swing_owns_body || soldier_render_anim.hit_reaction_kind ==
                                         Engine::Core::HitReactionKind::Recoil) &&
        soldier_render_anim.is_hit_reacting;
    if (swing_recoil_active) {
      soldier_render_anim.is_hit_reacting = false;
    }

    if (!is_mounted_spawn && guard_pose_amount(soldier_render_anim) > 0.0F &&
        soldier_render_anim.shield_formation_pose == ShieldFormationPose::None) {
      int const row = static_cast<int>(layout.row_index);
      int const col = static_cast<int>(layout.col_index);
      soldier_render_anim.shield_formation_pose = shared_guard_shield_pose(
          unit_comp,
          visual_spec,
          creature_presentation != nullptr &&
              creature_presentation->formation_guard_active,
          creature_presentation != nullptr && creature_presentation->guard_requested,
          creature_presentation != nullptr &&
              creature_presentation->defensive_layout_locked,
          row,
          col,
          rows,
          cols);
    }
    float const offset_x = layout.offset_x;
    float const offset_z = layout.offset_z;
    uint32_t const inst_seed = layout.inst_seed;
    float const phase_offset = layout.phase_offset;
    float const applied_yaw_offset = layout.yaw_offset + casualty_yaw;

    SoldierTurnSmoothingResult turn_smoothing{};
    QVector3D turn_slot_world;
    bool soldier_turn_smoothed = false;
    bool const soldier_is_casualty_body =
        soldier_render_anim.is_dying || soldier_render_anim.is_dead;
    if (turn_smoothing_active && !soldier_is_casualty_body &&
        static_cast<std::size_t>(idx) < layout_cache_comp->turn_states.size()) {
      turn_slot_world = unit_base.map(QVector3D(offset_x, 0.0F, offset_z));

      float const cap_jitter =
          0.92F + 0.16F * static_cast<float>(inst_seed % 97U) / 96.0F;
      SoldierTurnSmoothingInputs smoothing_inputs{};
      smoothing_inputs.target_x = turn_slot_world.x();
      smoothing_inputs.target_z = turn_slot_world.z();
      smoothing_inputs.formation_yaw_degrees =
          transform_comp->rotation.y + applied_yaw_offset;
      smoothing_inputs.dt = turn_smoothing_dt;
      smoothing_inputs.max_speed = turn_smoothing_cap * cap_jitter;
      smoothing_inputs.turn_rate_degrees = is_mounted_spawn ? 240.0F : 540.0F;
      smoothing_inputs.allow_travel_yaw = turn_smoothing_travel_yaw;
      turn_smoothing = resolve_soldier_turn_smoothing(
          layout_cache_comp->turn_states[static_cast<std::size_t>(idx)],
          smoothing_inputs);
      soldier_turn_smoothed = true;
      if (turn_smoothing.relocating && !soldier_render_anim.is_attacking &&
          !soldier_render_anim.is_constructing &&
          !Render::Creature::is_moving_animation(soldier_render_anim.movement_state)) {
        soldier_render_anim.movement_state = Animation::MovementState::Walk;
      }
    }

    bool const soldier_has_locomotion =
        Render::Creature::is_moving_animation(soldier_render_anim.movement_state);
    bool const soldier_is_running =
        Render::Creature::is_running_animation(soldier_render_anim.movement_state);

    QMatrix4x4 inst_model;
    float applied_yaw = applied_yaw_offset;

    if (transform_comp != nullptr && soldier_turn_smoothed) {
      applied_yaw = turn_smoothing.yaw_degrees;
      QMatrix4x4 m;
      m.translate(turn_smoothing.x, transform_comp->position.y, turn_smoothing.z);
      m.rotate(turn_smoothing.yaw_degrees, 0.0F, 1.0F, 0.0F);
      m.scale(
          transform_comp->scale.x, transform_comp->scale.y, transform_comp->scale.z);
      inst_model = m;
    } else if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + applied_yaw_offset;
      QMatrix4x4 m = unit_base;
      m.translate(offset_x + casualty_offset_x,
                  casualty_offset_y,
                  offset_z + casualty_offset_z);
      m.rotate(applied_yaw_offset, 0.0F, 1.0F, 0.0F);
      if (std::abs(casualty_pitch) > 0.01F || std::abs(casualty_roll) > 0.01F) {

        constexpr float k_casualty_tumble_pivot_y = 0.92F;
        m.translate(0.0F, k_casualty_tumble_pivot_y, 0.0F);
        m.rotate(casualty_pitch, 1.0F, 0.0F, 0.0F);
        m.rotate(casualty_roll, 0.0F, 0.0F, 1.0F);
        m.translate(0.0F, -k_casualty_tumble_pivot_y, 0.0F);
      }

      m.scale(
          transform_comp->scale.x, transform_comp->scale.y, transform_comp->scale.z);
      inst_model = m;
    } else {
      inst_model = ctx.model;
      inst_model.translate(offset_x, 0.0F, offset_z);
      inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
    }

    const QVector3D early_world_pos = inst_model.map(QVector3D(0.0F, 0.0F, 0.0F));

    constexpr float k_soldier_cull_center_y = 0.9F;
    constexpr float k_soldier_cull_radius = 1.4F;
    const QVector3D cull_center =
        inst_model.map(QVector3D(0.0F, k_soldier_cull_center_y, 0.0F));

    const auto visibility_index = static_cast<std::size_t>(idx);
    const std::uint32_t encoded_visible_frame =
        layout_cache_comp != nullptr &&
                visibility_index < layout_cache_comp->visibility_frames.size()
            ? layout_cache_comp->visibility_frames[visibility_index]
            : 0U;
    const bool recently_drawn =
        encoded_visible_frame != 0U && (frame_index - (encoded_visible_frame - 1U)) <=
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
    const bool outside_frustum = !visibility_result.in_frustum;
    const bool hidden_by_fog = !unit_fog_visible;

    const bool blocks_lens_gap =
        ctx.submission_visibility != nullptr &&
        ctx.submission_visibility->occludes_lens_gap(early_world_pos, ctx_entity_id);
    if (outside_frustum || hidden_by_fog || blocks_lens_gap) {
      if (outside_frustum) {
        ++s_render_stats.soldiers_skipped_frustum;
      } else if (hidden_by_fog) {
        ++s_render_stats.soldiers_skipped_fog;
      } else {
        ++s_render_stats.soldiers_skipped_lens_gap;
      }
      if (record_animation_diagnostics) {
        record_soldier_debug(
            idx,
            soldier_render_anim,
            soldier_render_anim,
            soldier_render_anim.combat_visual.attack_phase,
            Render::Creature::resolve_pose(soldier_render_anim).animation_state,
            HumanoidLOD::Billboard,
            outside_frustum ? Render::Profiling::SoldierCullReason::Frustum
            : hidden_by_fog ? Render::Profiling::SoldierCullReason::Fog
                            : Render::Profiling::SoldierCullReason::LensGap,
            false,
            early_world_pos,
            applied_yaw,
            1.0F,
            1.0F,
            0.0F,
            0.0F);
      }
      return;
    }

    auto const hit_reaction_transform =
        record_animation_diagnostics
            ? Animation::resolve_humanoid_hit_reaction_transform({
                  .active = soldier_render_anim.is_hit_reacting || swing_recoil_active,
                  .intensity = soldier_render_anim.hit_reaction_intensity,
                  .recoil_x = soldier_render_anim.hit_recoil_x,
                  .recoil_z = soldier_render_anim.hit_recoil_z,
                  .inst_seed = inst_seed,
              })
            : Animation::HumanoidHitReactionTransformSample{};

    inst_ctx.model = inst_model;

    VariationParams variation = VariationParams::from_seed(inst_seed);
    owner.adjust_variation(inst_ctx, inst_seed, variation);
    auto const locomotion_variation = Animation::resolve_humanoid_locomotion_variation({
        .has_locomotion = soldier_has_locomotion,
        .running = soldier_is_running,
        .walk_speed_multiplier = variation.walk_speed_mult,
        .arm_swing_amplitude = variation.arm_swing_amp,
        .stance_width = variation.stance_width,
        .posture_slump = variation.posture_slump,
    });
    variation.walk_speed_mult = locomotion_variation.walk_speed_multiplier;
    variation.arm_swing_amp = locomotion_variation.arm_swing_amplitude;
    variation.stance_width = locomotion_variation.stance_width;
    variation.posture_slump = locomotion_variation.posture_slump;

    float const combined_height_scale = height_scale * variation.height_scale;
    if (needs_height_scaling || std::abs(variation.height_scale - 1.0F) > 0.001F) {
      QMatrix4x4 scale_matrix;
      scale_matrix.scale(variation.bulk_scale, combined_height_scale, 1.0F);
      inst_ctx.model = inst_ctx.model * scale_matrix;
    }
    float const yaw_rad = qDegreesToRadians(applied_yaw);
    QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
    if (forward.lengthSquared() > 1e-8F) {
      forward.normalize();
    } else {
      forward = QVector3D(0.0F, 0.0F, 1.0F);
    }
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D right = QVector3D::crossProduct(up, forward);
    if (right.lengthSquared() > 1e-8F) {
      right.normalize();
    } else {
      right = QVector3D(1.0F, 0.0F, 0.0F);
    }

    VisualLocomotionSample const visual_locomotion =
        resolve_visual_locomotion_sample(visual_movement, forward);

    auto* locomotion_persistent_state = humanoid_anim_state;
    Render::Creature::SoldierCombatLaneState* cached_lane_state = nullptr;
    if (layout_cache_comp != nullptr && use_per_soldier_locomotion_state) {
      auto& animation_states = layout_cache_comp->animation_states;
      auto const state_index = static_cast<std::size_t>(idx);
      if (state_index < animation_states.size()) {
        locomotion_persistent_state = &animation_states[state_index];
      }
      auto& combat_lanes = layout_cache_comp->combat_lanes;
      if (state_index < combat_lanes.size()) {
        cached_lane_state = &combat_lanes[state_index];
      }
    }

    std::uint8_t base_attack_variant = soldier_render_anim.attack_variant;
    if (ctx.has_attack_variant_override) {
      base_attack_variant = ctx.attack_variant_override;
    }

    Render::Creature::CombatLaneInputs lane_inputs{};
    lane_inputs.unit_seed = seed;
    lane_inputs.soldier_seed = inst_seed;
    lane_inputs.row = static_cast<int>(layout.row_index);
    lane_inputs.col = static_cast<int>(layout.col_index);
    lane_inputs.rows = rows;
    lane_inputs.cols = cols;
    lane_inputs.health_ratio = unit_health_ratio;

    auto const* soldier_directive =
        has_shared_formation_layout
            ? &formation_presentation->soldiers[static_cast<std::size_t>(idx)]
            : nullptr;
    bool const formation_melee =
        soldier_directive != nullptr && formation_presentation->melee_ordered;
    bool const soldier_has_contact =
        !formation_melee || soldier_directive->opponent_id != 0U;
    bool const soldier_in_formation_fight = formation_fight_active &&
                                            soldier_directive != nullptr &&
                                            soldier_directive->alive;
    lane_inputs.local_enemy_pressure =
        formation_melee ? (soldier_has_contact ? 1.0F : 0.15F) : local_enemy_pressure;
    lane_inputs.force_single_soldier = ctx.force_single_soldier;

    lane_inputs.is_melee = soldier_render_anim.is_melee || soldier_in_formation_fight;
    lane_inputs.is_mounted = is_mounted_spawn;
    lane_inputs.attack_family = soldier_render_anim.attack_family;

    Render::Creature::SoldierCombatLaneState const lane_snapshot =
        (cached_lane_state != nullptr) ? *cached_lane_state
                                       : Render::Creature::SoldierCombatLaneState{};
    auto lane_resolution =
        Render::Creature::resolve_soldier_combat_lane(lane_snapshot, lane_inputs);
    if (soldier_in_formation_fight) {
      using CoreRole = Engine::Core::FormationSoldierCombatRole;
      using Lane = Render::Creature::SoldierCombatLane;
      Lane preferred_lane = lane_resolution.profile.lane;
      switch (soldier_directive->combat_role) {
      case CoreRole::LeadStrike:
        preferred_lane = Lane::LeadStrike;
        break;
      case CoreRole::SupportStrike:
        preferred_lane = Lane::SupportStrike;
        break;
      case CoreRole::Guard:
        preferred_lane = Lane::ShieldBrace;
        break;
      case CoreRole::StepIn:
        preferred_lane = Lane::StepIn;
        break;
      case CoreRole::StepOut:
        preferred_lane = Lane::StepOut;
        break;
      case CoreRole::Ready:
        preferred_lane = Lane::IdleReady;
        break;
      case CoreRole::None:
        break;
      }
      if (soldier_directive->damage_carrier) {
        preferred_lane = Lane::LeadStrike;
      }
      lane_resolution.state.lane = preferred_lane;
      lane_resolution.profile =
          Animation::profile_with_lane(lane_resolution.profile, preferred_lane);
      lane_resolution.profile.phase_bias += soldier_directive->combat_phase_bias;
    }
    if (allow_animation_persistence && cached_lane_state != nullptr) {
      *cached_lane_state = lane_resolution.state;
    }

    Render::Creature::CombatVisualPersistentState previous_combat_visual{};
    if (locomotion_persistent_state != nullptr) {
      previous_combat_visual = locomotion_persistent_state->combat_visual;
    }

    if (soldier_render_anim.is_hit_reacting &&
        soldier_render_anim.hit_reaction_kind !=
            Engine::Core::HitReactionKind::Stagger &&
        previous_combat_visual.active &&
        (previous_combat_visual.phase == Animation::CombatTransactionPhase::Strike ||
         previous_combat_visual.phase ==
             Animation::CombatTransactionPhase::FollowThrough)) {
      swing_recoil_active = true;
      soldier_render_anim.is_hit_reacting = false;
    }
    Render::Creature::CombatVisualRawInputs raw_combat{};
    raw_combat.sample_time =
        soldier_render_anim.time *
        (soldier_directive != nullptr ? soldier_directive->combat_speed_scale : 1.0F);
    raw_combat.attack_offset = soldier_render_anim.attack_offset;
    raw_combat.has_attack_offset = soldier_render_anim.has_attack_offset;

    bool visual_attack_requested =
        soldier_render_anim.is_attacking || soldier_in_formation_fight;
    bool const lock_only_attack =
        soldier_render_anim.attack_from_melee_lock &&
        soldier_render_anim.combat_phase == CombatAnimPhase::Idle &&
        !soldier_render_anim.has_authored_action_phase;
    bool const single_body_unit = ctx.force_single_soldier || total_layout_count <= 1;
    if (lock_only_attack && single_body_unit && soldier_directive == nullptr &&
        !soldier_in_formation_fight) {

      visual_attack_requested = false;
      soldier_render_anim.movement_state =
          Render::Creature::MovementAnimationState::Idle;
    }
    if (soldier_in_formation_fight && !soldier_directive->damage_carrier) {
      using Role = Engine::Core::FormationSoldierCombatRole;
      visual_attack_requested = soldier_directive->combat_role == Role::LeadStrike ||
                                soldier_directive->combat_role == Role::SupportStrike ||
                                soldier_directive->combat_role == Role::StepIn;
    }
    raw_combat.attack_requested = visual_attack_requested;
    raw_combat.is_melee = soldier_render_anim.is_melee || soldier_in_formation_fight;
    raw_combat.is_mounted = is_mounted_spawn;
    raw_combat.is_casting = soldier_render_anim.is_casting;
    raw_combat.finisher_attack = soldier_render_anim.finisher_attack;
    raw_combat.amplified_attack = false;
    raw_combat.is_hit_reacting = soldier_render_anim.is_hit_reacting;
    raw_combat.is_healing = soldier_render_anim.is_healing;
    raw_combat.is_constructing = soldier_render_anim.is_constructing;
    raw_combat.is_dying = soldier_render_anim.is_dying;
    raw_combat.is_dead = soldier_render_anim.is_dead;
    raw_combat.locomotion = soldier_render_anim.movement_state;
    bool const follows_authoritative_strike =
        soldier_directive == nullptr || soldier_directive->damage_carrier;
    raw_combat.combat_phase = follows_authoritative_strike
                                  ? soldier_render_anim.combat_phase
                                  : Render::GL::CombatAnimPhase::Idle;
    raw_combat.combat_phase_progress =
        follows_authoritative_strike ? soldier_render_anim.combat_phase_progress : 0.0F;
    raw_combat.attack_variant = base_attack_variant;
    raw_combat.attack_target_id =
        soldier_directive != nullptr && soldier_directive->opponent_id != 0U
            ? soldier_directive->opponent_id
            : unit_attack_target_id;
    raw_combat.attack_target_alive =
        soldier_directive != nullptr && soldier_directive->opponent_id != 0U
            ? true
            : unit_attack_target_alive;
    raw_combat.attack_family = soldier_render_anim.attack_family;
    raw_combat.authored_action_phase = soldier_render_anim.authored_action_phase;
    raw_combat.has_authored_action_phase =
        soldier_render_anim.has_authored_action_phase;

    auto combat_resolution = Render::Creature::resolve_combat_visual_state(
        previous_combat_visual, raw_combat, lane_resolution.profile);
    if (formation_melee ||
        (lock_only_attack && single_body_unit && soldier_directive == nullptr &&
         !soldier_in_formation_fight)) {
      combat_resolution.resolved.authoritative = true;
    }
    if (allow_animation_persistence && locomotion_persistent_state != nullptr) {
      locomotion_persistent_state->combat_visual = combat_resolution.persistent;
    }
    sync_combat_visual_inputs(soldier_render_anim, combat_resolution.resolved);
    bool const render_has_locomotion =
        Render::Creature::is_moving_animation(soldier_render_anim.movement_state);

    HumanoidLocomotionInputs locomotion_inputs{};
    locomotion_inputs.anim = soldier_render_anim;
    locomotion_inputs.variation = variation;
    locomotion_inputs.move_speed = visual_locomotion.speed;
    locomotion_inputs.entity_forward = forward;
    locomotion_inputs.locomotion_direction = visual_locomotion.direction;
    locomotion_inputs.movement_target = visual_locomotion.movement_target;
    locomotion_inputs.has_movement_target = visual_locomotion.has_movement_target;
    locomotion_inputs.animation_time = anim.time;
    locomotion_inputs.phase_offset = phase_offset;
    locomotion_inputs.persistent_state = locomotion_persistent_state;
    locomotion_inputs.allow_persistent_update = allow_animation_persistence;
    if (soldier_turn_smoothed && turn_smoothing.relocating) {

      locomotion_inputs.move_speed =
          std::max(locomotion_inputs.move_speed, turn_smoothing.travel_speed);
      float const travel_rad = qDegreesToRadians(turn_smoothing.travel_yaw_degrees);
      locomotion_inputs.locomotion_direction =
          QVector3D(std::sin(travel_rad), 0.0F, std::cos(travel_rad));
      locomotion_inputs.movement_target = turn_slot_world;
      locomotion_inputs.has_movement_target = true;
    }
    HumanoidLocomotionState locomotion_state =
        build_humanoid_locomotion_state(locomotion_inputs);
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
    auto const phase_override = Animation::resolve_humanoid_locomotion_phase_override({
        .bow_ready_idle = unit_is_archer,
        .has_locomotion = render_has_locomotion,
        .attacking = soldier_render_anim.is_attacking,
    });
    if (phase_override.active) {
      locomotion_state.gait.cycle_phase = phase_override.cycle_phase;
    }

    ConstructionRole construction_role = ConstructionRole::None;
    if (soldier_render_anim.is_constructing) {
      construction_role =
          resolve_construction_role(visual_spec,
                                    inst_seed,
                                    ctx.force_single_soldier,
                                    soldier_render_anim.construction_job);
    }

    HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = soldier_render_anim;
    anim_ctx.inputs.is_mounted = is_mounted_spawn;
    anim_ctx.variation = variation;
    anim_ctx.formation = formation;
    anim_ctx.jitter_seed = phase_offset;

    anim_ctx.entity_forward = forward;
    anim_ctx.entity_right = right;
    anim_ctx.entity_up = up;
    anim_ctx.locomotion_direction = locomotion_state.locomotion_direction;
    anim_ctx.yaw_degrees = applied_yaw;
    anim_ctx.yaw_radians = yaw_rad;
    anim_ctx.has_movement_target = locomotion_state.has_movement_target;
    anim_ctx.move_speed = locomotion_state.move_speed;
    anim_ctx.movement_target = locomotion_state.movement_target;
    anim_ctx.locomotion_velocity = locomotion_state.locomotion_velocity;
    anim_ctx.gait = locomotion_state.gait;
    anim_ctx.locomotion_cycle_time = locomotion_state.gait.cycle_time;
    anim_ctx.locomotion_phase = locomotion_state.gait.cycle_phase;
    anim_ctx.construction_role = construction_role;
    anim_ctx.attack_phase = anim_ctx.inputs.combat_visual.attack_phase;
    anim_ctx.attack_emphasis = anim_ctx.inputs.combat_visual.attack_emphasis;
    anim_ctx.amplified_attack = false;
    anim_ctx.finisher_attack = anim_ctx.inputs.combat_visual.finisher_attack;

    Animation::HumanoidAmbientRuntimeState previous_ambient{};
    if (locomotion_persistent_state != nullptr) {
      previous_ambient = locomotion_persistent_state->ambient_idle;
    }
    Animation::HumanoidAmbientSelectionInputs ambient_inputs{
        .ambient_state = previous_ambient,
        .sample_time = anim.time,
        .jump_active = commander_jump.active,
        .jump_phase = commander_jump.phase,
        .flag_rally_active = rally_pose.active,
        .flag_rally_phase = rally_pose.phase,
        .mounted = is_mounted_spawn,
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
        .seed = inst_seed,
        .idle_duration = anim_ctx.inputs.idle_duration,
    };
    auto const ambient_tick =
        Animation::resolve_humanoid_ambient_selection(ambient_inputs);

    if (allow_animation_persistence && locomotion_persistent_state != nullptr) {
      locomotion_persistent_state->ambient_idle = ambient_tick.state;
    }
    if (ambient_tick.sample.active) {
      anim_ctx.ambient_idle_type = ambient_tick.sample.type;
      anim_ctx.ambient_idle_phase = ambient_tick.sample.phase;
      anim_ctx.ambient_idle_blend = ambient_tick.sample.blend;
    }

    HumanoidPose const pose{};

    bool const prepare_shadow_grounding =
        RCP::pass_intent_from_ctx(inst_ctx) == RCP::RenderPassIntent::Shadow;
    bool const world_already_grounded =
        ctx.skip_ground_offset || prepare_shadow_grounding;
    float shadow_surface_world_y = 0.0F;
    bool shadow_surface_height_valid = false;
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
      float const grounded_contact_y = RCP::grounded_humanoid_contact_y(
          grounding_archetype, grounding_species, pose, anim_ctx);
      shadow_surface_world_y =
          RCP::ground_model_contact_to_surface(ctx.world_view.terrain_or_empty(),
                                               inst_ctx.model,
                                               grounded_contact_y,
                                               combined_height_scale,
                                               entity_ground_offset);
      shadow_surface_height_valid = true;
    } else if (!world_already_grounded) {
      constexpr float k_ground_cache_epsilon = 1.0e-3F;
      SoldierGroundSample* ground_sample = nullptr;
      if (layout_cache_comp != nullptr &&
          visibility_index < layout_cache_comp->ground_samples.size()) {
        ground_sample = &layout_cache_comp->ground_samples[visibility_index];
      }
      QVector3D const pre_ground_origin = RCP::model_world_origin(inst_ctx.model);
      if (ground_sample != nullptr && ground_sample->valid &&
          std::abs(ground_sample->x - pre_ground_origin.x()) < k_ground_cache_epsilon &&
          std::abs(ground_sample->z - pre_ground_origin.z()) < k_ground_cache_epsilon) {
        RCP::set_model_world_y(inst_ctx.model, ground_sample->model_y);
        shadow_surface_world_y = ground_sample->surface_y;
      } else {
        shadow_surface_world_y =
            RCP::ground_model_contact_to_surface(ctx.world_view.terrain_or_empty(),
                                                 inst_ctx.model,
                                                 0.0F,
                                                 combined_height_scale,
                                                 entity_ground_offset);
        if (ground_sample != nullptr) {
          ground_sample->x = pre_ground_origin.x();
          ground_sample->z = pre_ground_origin.z();
          ground_sample->model_y = RCP::model_world_origin(inst_ctx.model).y();
          ground_sample->surface_y = shadow_surface_world_y;
          ground_sample->valid = true;
        }
      }
      shadow_surface_height_valid = true;
    }
    if (commander_jump.height_offset > 0.0F) {
      RCP::set_model_world_y(inst_ctx.model,
                             RCP::model_world_origin(inst_ctx.model).y() +
                                 commander_jump.height_offset);
    }
    if (casualty_offset_y > 0.0F) {
      RCP::set_model_world_y(inst_ctx.model,
                             RCP::model_world_origin(inst_ctx.model).y() +
                                 casualty_offset_y);
    }
    if (!soldier_is_casualty_body && !is_mounted_spawn &&
        !soldier_render_anim.has_authored_action_phase &&
        !soldier_render_anim.is_in_hold_mode && !soldier_render_anim.is_constructing) {
      float recoil_dir_x = soldier_render_anim.hit_recoil_x;
      float recoil_dir_z = soldier_render_anim.hit_recoil_z;
      if (std::abs(recoil_dir_x) < 1.0e-5F && std::abs(recoil_dir_z) < 1.0e-5F) {
        recoil_dir_x = -forward.x();
        recoil_dir_z = -forward.z();
      }
      auto const root_motion = Animation::resolve_combat_root_motion({
          .attacking = soldier_render_anim.combat_visual.active &&
                       soldier_render_anim.is_attacking,
          .melee = soldier_render_anim.combat_visual.is_melee,
          .mounted = is_mounted_spawn,
          .formation_member = soldier_directive != nullptr &&
                              !ctx.force_single_soldier &&
                              !soldier_directive->damage_carrier,
          .phase = soldier_render_anim.combat_visual.phase,
          .attack_phase = soldier_render_anim.combat_visual.attack_phase,
          .attack_family = soldier_render_anim.combat_visual.attack_family,
          .swing_outcome = static_cast<Animation::MeleeSwingOutcome>(
              soldier_render_anim.attack_exchange_outcome),
          .hit_reacting = soldier_render_anim.is_hit_reacting || swing_recoil_active,
          .reaction = Animation::hit_reaction_form_from_kind(
              static_cast<std::uint8_t>(soldier_render_anim.hit_reaction_kind)),
          .reaction_progress = soldier_render_anim.hit_reaction_progress,
          .reaction_intensity = hit_reaction_base_intensity,
          .recoil_dir_x = recoil_dir_x,
          .recoil_dir_z = recoil_dir_z,
          .body_displaced_by_simulation =
              soldier_directive == nullptr && !has_shared_formation_layout,
          .seed = inst_seed,
      });
      if (root_motion.active) {
        QVector3D const root_origin = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
        QMatrix4x4 root;
        root.translate(
            root_motion.world_offset_x + forward.x() * root_motion.forward_offset,
            0.0F,
            root_motion.world_offset_z + forward.z() * root_motion.forward_offset);
        root.translate(root_origin);
        if (std::abs(root_motion.pitch_degrees) > 0.01F) {
          root.rotate(root_motion.pitch_degrees, right);
        }
        if (std::abs(root_motion.roll_degrees) > 0.01F) {
          root.rotate(root_motion.roll_degrees, forward);
        }
        if (root_motion.squash > 0.001F) {
          root.scale(1.0F, 1.0F - root_motion.squash, 1.0F);
        }
        root.translate(-root_origin);
        inst_ctx.model = root * inst_ctx.model;
      }
    }
    anim_ctx.instance_position = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    QVector3D const soldier_world_pos = anim_ctx.instance_position;
    float root_scale_y = 1.0F;
    float root_up_y = 1.0F;
    float root_tilt_degrees = 0.0F;
    if (record_animation_diagnostics) {
      QVector3D const model_up = inst_ctx.model.mapVector(QVector3D(0.0F, 1.0F, 0.0F));
      root_scale_y = model_up.length();
      QVector3D const normalized_model_up = root_scale_y > 1.0e-6F
                                                ? model_up / root_scale_y
                                                : QVector3D(0.0F, 1.0F, 0.0F);
      root_up_y = std::clamp(normalized_model_up.y(), -1.0F, 1.0F);
      root_tilt_degrees = qRadiansToDegrees(std::acos(root_up_y));
    }

    RCP::HumanoidLodStateInputs lod_inputs{};
    lod_inputs.ctx = &ctx;
    lod_inputs.soldier_world_pos = soldier_world_pos;
    lod_inputs.config = lod_config;
    const auto lod_state = RCP::resolve_humanoid_lod_state(lod_inputs);
    const auto lod_decision = lod_state.decision;
    if (lod_decision.culled) {
      ++s_render_stats.soldiers_skipped_lod;
      if (record_animation_diagnostics) {
        record_soldier_debug(
            idx,
            soldier_render_anim,
            anim_ctx.inputs,
            anim_ctx.attack_phase,
            Render::Creature::resolve_pose(anim_ctx.inputs).animation_state,
            static_cast<HumanoidLOD>(lod_decision.lod),
            Render::Profiling::SoldierCullReason::Billboard,
            false,
            soldier_world_pos,
            applied_yaw,
            root_up_y,
            root_scale_y,
            root_tilt_degrees,
            hit_reaction_transform.tilt_degrees);
      }
      return;
    }
    auto const soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

    anim_ctx.idle_breath_phase =
        phase_override.active
            ? locomotion_state.gait.cycle_phase
            : RCP::humanoid_idle_breath_phase_for_lod(
                  anim.time, inst_seed, soldier_lod, ctx.prewarming_via_runtime_path);

    ++s_render_stats.soldiers_rendered;

    RCP::CreatureGraphInputs graph_inputs{};
    graph_inputs.ctx = &inst_ctx;
    graph_inputs.anim = &soldier_render_anim;
    graph_inputs.entity = ctx.entity;
    graph_inputs.unit = unit_comp;
    graph_inputs.transform = transform_comp;
    auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
    graph_output.instance_index = static_cast<std::uint16_t>(idx);
    graph_output.spec = RCP::finalize_visible_humanoid_spec(
        visual_spec, soldier_render_anim, render_has_locomotion);
    graph_output.seed = inst_seed;
    graph_output.world_already_grounded = world_already_grounded;
    bool const selection_steady = humanoid_selection_is_steady(anim_ctx);
    SoldierSelectionCache* selection_slot =
        (layout_cache_comp != nullptr &&
         visibility_index < layout_cache_comp->selection_cache.size())
            ? &layout_cache_comp->selection_cache[visibility_index]
            : nullptr;
    bool const selection_refresh_due =
        ((frame_index + inst_seed) % k_selection_refresh_period) == 0U;
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
    } else {
      graph_output.humanoid_selection = RCP::resolve_humanoid_animation_selection(
          graph_output.spec, anim_ctx, graph_output.seed, &variant);
      if (selection_slot != nullptr) {
        selection_slot->valid = selection_steady;
        if (selection_steady) {
          selection_slot->archetype = graph_output.spec.archetype_id;
          selection_slot->movement_state =
              static_cast<std::uint8_t>(anim_ctx.inputs.movement_state);
          selection_slot->selection = *graph_output.humanoid_selection;
        }
      }
    }

    bool const transient_recovery_override =
        (soldier_render_anim.is_attacking != anim_ctx.inputs.is_attacking) ||
        (soldier_render_anim.combat_phase != anim_ctx.inputs.combat_phase) ||
        (std::abs(soldier_render_anim.combat_phase_progress -
                  anim_ctx.inputs.combat_phase_progress) > 1.0e-4F);
    if (layout_cache_comp != nullptr &&
        visibility_index < layout_cache_comp->visibility_frames.size()) {
      layout_cache_comp->visibility_frames[visibility_index] = frame_index + 1U;
    }
    if (record_animation_diagnostics) {
      record_soldier_debug(idx,
                           soldier_render_anim,
                           anim_ctx.inputs,
                           anim_ctx.attack_phase,
                           graph_output.humanoid_selection->state,
                           soldier_lod,
                           Render::Profiling::SoldierCullReason::None,
                           transient_recovery_override,
                           soldier_world_pos,
                           applied_yaw,
                           root_up_y,
                           root_scale_y,
                           root_tilt_degrees,
                           hit_reaction_transform.tilt_degrees);
    }

    RCP::HumanoidShadowStateInputs shadow_inputs{};
    shadow_inputs.ctx = &inst_ctx;
    shadow_inputs.graph = &graph_output;
    shadow_inputs.unit = unit_comp;
    shadow_inputs.soldier_world_pos = soldier_world_pos;
    shadow_inputs.lod = soldier_lod;
    shadow_inputs.camera_distance = lod_state.camera_distance;
    shadow_inputs.mounted = is_mounted_spawn;
    shadow_inputs.formation_id = ctx.entity != nullptr ? ctx.entity->get_id() : 0U;
    shadow_inputs.standing_idle =
        !is_mounted_spawn &&
        soldier_render_anim.movement_state ==
            Render::Creature::MovementAnimationState::Idle &&
        !soldier_render_anim.is_attacking && !soldier_render_anim.is_hit_reacting &&
        !soldier_render_anim.is_dying && !soldier_render_anim.is_dead &&
        !commander_jump.active;
    shadow_inputs.surface_world_y = shadow_surface_world_y;
    shadow_inputs.surface_height_valid = shadow_surface_height_valid;
    const auto shadow_state = RCP::prepare_humanoid_shadow_state(shadow_inputs);
    if (shadow_state.enabled) {
      if (out.shadow_batch.empty()) {
        out.shadow_batch.init(
            shadow_state.shader, shadow_state.mesh, shadow_state.light_dir);
      }
      out.shadow_batch.add(shadow_state.model, shadow_state.alpha, shadow_state.pass);
    }

    switch (soldier_lod) {
    case HumanoidLOD::Full: {

      ++s_render_stats.lod_full;

      if (is_mounted_spawn) {
        owner.append_companion_preparation(
            inst_ctx, variant, pose, anim_ctx, inst_seed, graph_output.lod, out);
        break;
      }

      out.bodies.add_humanoid(graph_output, pose, variant, anim_ctx);
      owner.append_companion_preparation(
          inst_ctx, variant, pose, anim_ctx, inst_seed, graph_output.lod, out);
      break;
    }

    case HumanoidLOD::Minimal: {

      ++s_render_stats.lod_minimal;
      if (is_mounted_spawn &&
          (ctx.template_prewarm ||
           (!ctx.allow_template_cache && ctx.suppress_animation_state_persistence))) {
        owner.append_companion_preparation(
            inst_ctx, variant, pose, anim_ctx, inst_seed, graph_output.lod, out);
        break;
      }
      out.bodies.add_humanoid(graph_output, pose, variant, anim_ctx);
      break;
    }

    case HumanoidLOD::Billboard:

      break;
    }
  };

  for (int const idx : live_slot_indices) {
    append_prepared_soldier(idx, anim);
  }

  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    for (const auto& entry : casualties_comp->entries) {
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
      }
      const CasualtyLaunch launch = resolve_casualty_launch(entry);
      auto const& casualty_layout =
          soldier_layouts[static_cast<std::size_t>(entry.slot_index)];
      float const anchor_offset_x =
          entry.has_local_anchor ? entry.local_x - casualty_layout.offset_x : 0.0F;
      float const anchor_offset_z =
          entry.has_local_anchor ? entry.local_z - casualty_layout.offset_z : 0.0F;
      float const anchor_yaw =
          entry.has_local_anchor ? entry.local_yaw - casualty_layout.yaw_offset : 0.0F;
      append_prepared_soldier(static_cast<int>(entry.slot_index),
                              casualty_anim,
                              anchor_offset_x + launch.x,
                              launch.y,
                              anchor_offset_z + launch.z,
                              launch.pitch,
                              launch.roll,
                              anchor_yaw);
    }
  }
}

} // namespace Render::Humanoid
