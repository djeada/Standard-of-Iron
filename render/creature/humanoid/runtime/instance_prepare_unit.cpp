#include "render/creature/humanoid/runtime/instance_prepare_internal.h"

namespace Render::Humanoid {

namespace instance_prepare_detail {

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

} // namespace instance_prepare_detail

} // namespace Render::Humanoid
