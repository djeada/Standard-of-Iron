#include "../submission_visibility.h"
#include "prepare_internal.h"

namespace Render::Humanoid {

void prepare_humanoid_instances(const HumanoidRendererBase& owner,
                                const DrawContext& ctx,
                                const AnimationInputs& anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation& out) {
  using namespace Render::GL;

  if (ctx.entity != nullptr && ctx.world == nullptr) {

    Engine::Core::publish_creature_presentation(ctx.entity, nullptr);
  }

#if defined(SOI_ENABLE_RUNTIME_TRACING)
  auto& profile = Render::Profiling::global_profile();
  Render::Profiling::AccumulatorScope const prepare_scope(
      ctx.template_prewarm ? nullptr : &profile.humanoid_preparation_us);
#endif

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
  std::vector<int> live_slot_indices;
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
  } else {
    int const live_count =
        unit_comp != nullptr
            ? Engine::Core::resolve_surviving_individual_count(
                  unit_comp->health, unit_comp->max_health, total_layout_count)
            : total_layout_count;
    int const first_live = std::max(0, total_layout_count - live_count);
    for (int index = first_live; index < total_layout_count; ++index) {
      live_slot_indices.push_back(index);
    }
  }
  if (has_entity_death && live_slot_indices.empty()) {
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
                      [total_layout_count, &live_slot_indices](const auto& entry) {
                        return entry.slot_index < total_layout_count &&
                               std::find(live_slot_indices.begin(),
                                         live_slot_indices.end(),
                                         static_cast<int>(entry.slot_index)) ==
                                   live_slot_indices.end();
                      }));
  }

  HumanoidVariant variant;
  owner.get_variant(ctx, seed, variant);
  seed_missing_humanoid_wear(variant, seed);

  if (!owner.m_proportion_scale_cached) {
    owner.m_cached_proportion_scale = owner.get_proportion_scaling();
    owner.m_proportion_scale_cached = true;
  }
  const QVector3D prop_scale = owner.m_cached_proportion_scale;
  const float height_scale = prop_scale.y();
  const bool needs_height_scaling = std::abs(height_scale - 1.0F) > 0.001F;

  const QMatrix4x4 k_identity_matrix;

  using Nation = FormationCalculatorFactory::Nation;
  using UnitCategory = FormationCalculatorFactory::UnitCategory;
  Nation nation = Nation::Roman;
  UnitCategory category =
      is_mounted_spawn ? UnitCategory::Cavalry : UnitCategory::Infantry;
  if (unit_comp != nullptr) {
    auto const definition =
        Game::Systems::FormationCombat::resolve_definition(*unit_comp);
    nation = definition.type;
    category = definition.category;
  }

  if (unit_comp != nullptr &&
      unit_comp->spawn_type == Game::Units::SpawnType::Builder &&
      anim.is_constructing) {
    category = UnitCategory::BuilderConstruction;
  }

  const IFormationCalculator* formation_calculator =
      FormationCalculatorFactory::get_calculator(nation, category);

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
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    Render::Profiling::AccumulatorScope const layout_scope(
        ctx.template_prewarm ? nullptr : &profile.soldier_layout_generation_us);
#endif

    if (layout_cache_comp != nullptr && layout_cache_comp->valid) {
      auto& entry = *layout_cache_comp;
      preserve_soldier_state_prefix =
          entry.seed == seed && entry.rows == rows && entry.cols == cols &&
          entry.layout_version == k_humanoid_layout_cache_version &&
          entry.formation.individuals_per_unit == formation.individuals_per_unit &&
          entry.formation.max_per_row == formation.max_per_row &&
          entry.formation.spacing == formation.spacing && entry.nation == nation &&
          entry.category == category;
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
        SoldierLayoutInputs inputs{};
        inputs.idx = idx;
        inputs.row = rows - 1 - idx / cols;
        inputs.col = idx % cols;
        inputs.rows = rows;
        inputs.cols = cols;
        inputs.formation_spacing = formation.spacing;
        inputs.seed = seed;
        inputs.force_single_soldier = ctx.force_single_soldier;
        inputs.melee_attack = anim.is_attacking && anim.is_melee;
        inputs.animation_time = anim.time;
        generated_layouts.push_back(
            build_soldier_layout(*formation_calculator, inputs));
      }

      if (layout_cache_comp != nullptr) {
        layout_cache_comp->formation = formation;
        layout_cache_comp->nation = nation;
        layout_cache_comp->category = category;
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

  if (has_shared_formation_layout && !anim.is_constructing &&
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
  }

  bool const use_per_soldier_locomotion_state = total_layout_count > 1;
  if (layout_cache_comp != nullptr && use_per_soldier_locomotion_state) {
    auto& animation_states = layout_cache_comp->animation_states;
    auto& combat_lanes = layout_cache_comp->combat_lanes;
    std::size_t const previous_state_count = animation_states.size();
    bool const state_count_changed =
        animation_states.size() != static_cast<std::size_t>(total_layout_count);
    animation_states.resize(static_cast<std::size_t>(total_layout_count));
    combat_lanes.resize(static_cast<std::size_t>(total_layout_count));
    if (!preserve_soldier_state_prefix) {
      for (auto& state : animation_states) {
        reset_humanoid_locomotion_state(state);
      }
      for (auto& lane_state : combat_lanes) {
        lane_state = {};
      }
    } else if (state_count_changed) {
      for (std::size_t idx = previous_state_count;
           idx < static_cast<std::size_t>(total_layout_count);
           ++idx) {
        reset_humanoid_locomotion_state(animation_states[idx]);
        combat_lanes[idx] = {};
      }
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
  auto record_soldier_debug = [&](int idx,
                                  const AnimationInputs&,
                                  const AnimationInputs& resolved_anim,
                                  float attack_phase,
                                  Render::Creature::AnimationStateId animation_state,
                                  HumanoidLOD lod,
                                  Render::Profiling::SoldierCullReason cull_reason,
                                  bool transient_recovery_override,
                                  const QVector3D& root_position,
                                  float root_up_y,
                                  float root_scale_y,
                                  float root_tilt_degrees,
                                  float hit_reaction_tilt_degrees) {
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    if (ctx.template_prewarm || ctx.entity == nullptr) {
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
    sample.root_up_y = root_up_y;
    sample.root_scale_y = root_scale_y;
    sample.root_tilt_degrees = root_tilt_degrees;
    sample.hit_reaction_tilt_degrees = hit_reaction_tilt_degrees;
    Render::Profiling::CombatAnimationDiagnostics::instance().record_soldier_sample(
        ctx.entity->get_id(), sample);
#else
    (void)idx;
    (void)resolved_anim;
    (void)attack_phase;
    (void)animation_state;
    (void)lod;
    (void)cull_reason;
    (void)transient_recovery_override;
    (void)root_position;
    (void)root_up_y;
    (void)root_scale_y;
    (void)root_tilt_degrees;
    (void)hit_reaction_tilt_degrees;
#endif
  };

  bool const formation_fight_active = has_shared_formation_layout &&
                                      formation_presentation->melee_ordered &&
                                      formation_presentation->target_alive;

  auto append_prepared_soldier = [&](int idx,
                                     const AnimationInputs& soldier_anim,
                                     float casualty_offset_x = 0.0F,
                                     float casualty_offset_y = 0.0F,
                                     float casualty_offset_z = 0.0F,
                                     float casualty_pitch = 0.0F,
                                     float casualty_roll = 0.0F) {
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
    if (!is_mounted_spawn && guard_pose_amount(soldier_render_anim) > 0.0F &&
        soldier_render_anim.shield_formation_pose == ShieldFormationPose::None) {
      auto const visual_spec = owner.visual_spec();
      int const row = static_cast<int>(layout.row_index);
      int const col = idx % cols;
      soldier_render_anim.shield_formation_pose = shared_guard_shield_pose(
          unit_comp,
          visual_spec,
          creature_presentation != nullptr &&
              creature_presentation->formation_guard_active,
          creature_presentation != nullptr && creature_presentation->guard_requested,
          row,
          col,
          rows,
          cols);
    }
    float const offset_x = layout.offset_x;
    float const offset_z = layout.offset_z;
    uint32_t const inst_seed = layout.inst_seed;
    float const phase_offset = layout.phase_offset;
    float const applied_yaw_offset = layout.yaw_offset;
    auto const commander_jump = preparation_mode.jump_pose();
    bool const soldier_has_locomotion =
        Render::Creature::is_moving_animation(soldier_render_anim.movement_state);
    bool const soldier_is_running =
        Render::Creature::is_running_animation(soldier_render_anim.movement_state);

    QMatrix4x4 inst_model;
    float applied_yaw = applied_yaw_offset;

    if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + applied_yaw_offset;
      QMatrix4x4 m = k_identity_matrix;
      m.translate(transform_comp->position.x,
                  transform_comp->position.y,
                  transform_comp->position.z);
      m.rotate(transform_comp->rotation.y, 0.0F, 1.0F, 0.0F);
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
    const auto visibility_result =
        ctx.submission_visibility != nullptr
            ? ctx.submission_visibility->evaluate_sphere(
                  cull_center, k_soldier_cull_radius, ctx.submission_fog_mode)
            : SubmissionVisibilityResult{
                  ctx.camera == nullptr ||
                      ctx.camera->is_in_frustum(cull_center, k_soldier_cull_radius),
                  true};
    const bool outside_frustum = !visibility_result.in_frustum;
    const bool hidden_by_fog = !visibility_result.fog_visible;
    if (outside_frustum || hidden_by_fog) {
      if (outside_frustum) {
        ++s_render_stats.soldiers_skipped_frustum;
      } else {
        ++s_render_stats.soldiers_skipped_fog;
      }
#if defined(SOI_ENABLE_RUNTIME_TRACING)
      record_soldier_debug(
          idx,
          soldier_render_anim,
          soldier_render_anim,
          soldier_render_anim.combat_visual.attack_phase,
          Render::Creature::resolve_pose(soldier_render_anim).animation_state,
          HumanoidLOD::Billboard,
          outside_frustum ? Render::Profiling::SoldierCullReason::Frustum
                          : Render::Profiling::SoldierCullReason::Fog,
          false,
          early_world_pos,
          1.0F,
          1.0F,
          0.0F,
          0.0F);
#endif
      return;
    }

    auto const hit_reaction_transform =
        Animation::resolve_humanoid_hit_reaction_transform({
            .active = soldier_render_anim.is_hit_reacting,
            .intensity = soldier_render_anim.hit_reaction_intensity,
            .recoil_x = soldier_render_anim.hit_recoil_x,
            .recoil_z = soldier_render_anim.hit_recoil_z,
            .inst_seed = inst_seed,
        });

    DrawContext inst_ctx = ctx;
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
        !formation_melee ||
        soldier_directive->action == Engine::Core::FormationSoldierAction::MeleeEngaged;
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

      constexpr float k_centered_layout_phase = 0.125F;
      constexpr float k_combat_stagger_scale = 0.55F;
      lane_resolution.profile.phase_bias +=
          (phase_offset - k_centered_layout_phase) * k_combat_stagger_scale;
    }
    if (allow_animation_persistence && cached_lane_state != nullptr) {
      *cached_lane_state = lane_resolution.state;
    }

    Render::Creature::CombatVisualPersistentState previous_combat_visual{};
    if (locomotion_persistent_state != nullptr) {
      previous_combat_visual = locomotion_persistent_state->combat_visual;
    }
    Render::Creature::CombatVisualRawInputs raw_combat{};
    raw_combat.sample_time = soldier_render_anim.time;
    raw_combat.attack_offset = soldier_render_anim.attack_offset;
    raw_combat.has_attack_offset = soldier_render_anim.has_attack_offset;

    raw_combat.attack_requested =
        soldier_render_anim.is_attacking || soldier_in_formation_fight;
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
    raw_combat.combat_phase = soldier_render_anim.combat_phase;
    raw_combat.combat_phase_progress = soldier_render_anim.combat_phase_progress;
    raw_combat.attack_variant = base_attack_variant;
    raw_combat.attack_target_id = unit_attack_target_id;
    raw_combat.attack_target_alive = unit_attack_target_alive;
    raw_combat.attack_family = soldier_render_anim.attack_family;

    auto combat_resolution = Render::Creature::resolve_combat_visual_state(
        previous_combat_visual, raw_combat, lane_resolution.profile);
    if (formation_melee) {
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
    HumanoidLocomotionState locomotion_state =
        build_humanoid_locomotion_state(locomotion_inputs);
    auto const locomotion_override =
        Animation::resolve_humanoid_locomotion_action_override({
            .commander_jump_active = commander_jump.active,
        });
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
        .bow_ready_idle = unit_comp != nullptr &&
                          unit_comp->spawn_type == Game::Units::SpawnType::Archer,
        .has_locomotion = render_has_locomotion,
        .attacking = soldier_render_anim.is_attacking,
    });
    if (phase_override.active) {
      locomotion_state.gait.cycle_phase = phase_override.cycle_phase;
    }

    auto const visual_spec = owner.visual_spec();
    ConstructionRole construction_role = ConstructionRole::None;
    if (soldier_render_anim.is_constructing) {
      construction_role =
          resolve_construction_role(visual_spec, inst_seed, ctx.force_single_soldier);
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

    auto const rally_pose = preparation_mode.flag_rally_pose();
    auto const ambient_selection = Animation::resolve_humanoid_ambient_selection({
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
        .seed = inst_seed,
        .idle_duration = anim_ctx.inputs.idle_duration,
    });
    if (ambient_selection.active) {
      anim_ctx.ambient_idle_type = ambient_selection.type;
      anim_ctx.ambient_idle_phase = ambient_selection.phase;
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
          RCP::ground_model_contact_to_surface(inst_ctx.model,
                                               grounded_contact_y,
                                               combined_height_scale,
                                               entity_ground_offset);
      shadow_surface_height_valid = true;
    } else if (!world_already_grounded) {
      shadow_surface_world_y = RCP::ground_model_contact_to_surface(
          inst_ctx.model, 0.0F, combined_height_scale, entity_ground_offset);
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
    anim_ctx.instance_position = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    QVector3D const soldier_world_pos = anim_ctx.instance_position;
    QVector3D const model_up = inst_ctx.model.mapVector(QVector3D(0.0F, 1.0F, 0.0F));
    float const root_scale_y = model_up.length();
    QVector3D const normalized_model_up =
        root_scale_y > 1.0e-6F ? model_up / root_scale_y : QVector3D(0.0F, 1.0F, 0.0F);
    float const root_up_y = std::clamp(normalized_model_up.y(), -1.0F, 1.0F);
    float const root_tilt_degrees = qRadiansToDegrees(std::acos(root_up_y));

    RCP::HumanoidLodStateInputs lod_inputs{};
    lod_inputs.ctx = &ctx;
    lod_inputs.soldier_world_pos = soldier_world_pos;
    lod_inputs.config = lod_config;
    lod_inputs.frame_index = frame_index;
    lod_inputs.instance_seed = inst_seed;
    const auto lod_state = RCP::resolve_humanoid_lod_state(lod_inputs);
    const auto lod_decision = lod_state.decision;
    if (lod_decision.culled) {
      auto const cull_reason = lod_decision.reason == RCP::CullReason::Temporal
                                   ? Render::Profiling::SoldierCullReason::Temporal
                                   : Render::Profiling::SoldierCullReason::Billboard;
      if (lod_decision.reason == RCP::CullReason::Billboard) {
        ++s_render_stats.soldiers_skipped_lod;
      } else if (lod_decision.reason == RCP::CullReason::Temporal) {
        ++s_render_stats.soldiers_skipped_temporal;
      }
      record_soldier_debug(
          idx,
          soldier_render_anim,
          anim_ctx.inputs,
          anim_ctx.attack_phase,
          Render::Creature::resolve_pose(anim_ctx.inputs).animation_state,
          static_cast<HumanoidLOD>(lod_decision.lod),
          cull_reason,
          false,
          soldier_world_pos,
          root_up_y,
          root_scale_y,
          root_tilt_degrees,
          hit_reaction_transform.tilt_degrees);
      return;
    }
    auto const soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

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
        visual_spec, variant, soldier_render_anim, render_has_locomotion);
    graph_output.seed = inst_seed;
    graph_output.world_already_grounded = world_already_grounded;
    graph_output.humanoid_selection = RCP::resolve_humanoid_animation_selection(
        graph_output.spec, anim_ctx, graph_output.seed, &variant);
    bool const transient_recovery_override =
        (soldier_render_anim.is_attacking != anim_ctx.inputs.is_attacking) ||
        (soldier_render_anim.combat_phase != anim_ctx.inputs.combat_phase) ||
        (std::abs(soldier_render_anim.combat_phase_progress -
                  anim_ctx.inputs.combat_phase_progress) > 1.0e-4F);
    record_soldier_debug(idx,
                         soldier_render_anim,
                         anim_ctx.inputs,
                         anim_ctx.attack_phase,
                         graph_output.humanoid_selection->state,
                         soldier_lod,
                         Render::Profiling::SoldierCullReason::None,
                         transient_recovery_override,
                         soldier_world_pos,
                         root_up_y,
                         root_scale_y,
                         root_tilt_degrees,
                         hit_reaction_transform.tilt_degrees);

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

      RCP::PreparedHumanoidBodyState body_state;
      body_state.graph = graph_output;
      body_state.pose = pose;
      body_state.variant = variant;
      body_state.animation = anim_ctx;
      out.bodies.add_humanoid(body_state);
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
      RCP::PreparedHumanoidBodyState body_state;
      body_state.graph = graph_output;
      body_state.pose = pose;
      body_state.variant = variant;
      body_state.animation = anim_ctx;
      out.bodies.add_humanoid(body_state);
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
      float launch_x = 0.0F;
      float launch_y = 0.0F;
      float launch_z = 0.0F;
      float launch_pitch = 0.0F;
      float launch_roll = 0.0F;
      if (entry.launched) {
        float const casualty_age =
            entry.state == Engine::Core::DeathSequenceState::Dying
                ? entry.state_time
                : entry.state_duration + entry.state_time;
        constexpr float k_charge_casualty_flight_seconds = 1.45F;
        float const flight_time =
            std::min(casualty_age, k_charge_casualty_flight_seconds);
        launch_x = entry.launch_velocity_x * flight_time;
        launch_z = entry.launch_velocity_z * flight_time;
        launch_y = std::max(0.0F,
                            entry.launch_velocity_y * flight_time -
                                4.9F * flight_time * flight_time);
        launch_pitch = entry.launch_pitch_speed * flight_time;
        launch_roll = entry.launch_roll_speed * flight_time;
      }
      append_prepared_soldier(static_cast<int>(entry.slot_index),
                              casualty_anim,
                              launch_x,
                              launch_y,
                              launch_z,
                              launch_pitch,
                              launch_roll);
    }
  }
}

} // namespace Render::Humanoid
