#include "prepare_internal.h"

namespace Render::Humanoid {

void prepare_humanoid_instances(const HumanoidRendererBase& owner,
                                const DrawContext& ctx,
                                const AnimationInputs& anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation& out) {
  using namespace Render::GL;

  auto& profile = Render::Profiling::global_profile();
  Render::Profiling::AccumulatorScope const prepare_scope(
      ctx.template_prewarm ? nullptr : &profile.humanoid_preparation_us);

  FormationParams const formation = HumanoidRendererBase::resolve_formation(owner, ctx);

  Engine::Core::UnitComponent* unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  }

  Engine::Core::TransformComponent* transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    transform_comp = ctx.entity->get_component<Engine::Core::TransformComponent>();
  }
  Engine::Core::FormationModeComponent* formation_mode = nullptr;
  if (ctx.entity != nullptr) {
    formation_mode = ctx.entity->get_component<Engine::Core::FormationModeComponent>();
  }
  Engine::Core::GuardModeComponent* guard_mode_comp = nullptr;
  if (ctx.entity != nullptr) {
    guard_mode_comp = ctx.entity->get_component<Engine::Core::GuardModeComponent>();
  }

  float entity_ground_offset =
      owner.resolve_entity_ground_offset(ctx, unit_comp, transform_comp);

  uint32_t seed = 0U;
  if (ctx.has_seed_override) {
    seed = ctx.seed_override;
  } else {
    if (unit_comp != nullptr) {
      seed ^= uint32_t(unit_comp->owner_id * 2654435761U);
    }
    if (ctx.entity != nullptr) {
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
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
  int live_count = total_layout_count;
  if (!ctx.force_single_soldier && unit_comp != nullptr) {
    live_count = Engine::Core::resolve_surviving_individual_count(
        unit_comp->health, unit_comp->max_health, formation.individuals_per_unit);
  }
  bool const has_entity_death = anim.is_dying || anim.is_dead;
  int const visible_count = has_entity_death ? std::max(1, live_count) : live_count;
  auto* casualties_comp =
      (ctx.entity != nullptr)
          ? ctx.entity->get_component<Engine::Core::SoldierCasualtyAnimationComponent>()
          : nullptr;
  std::size_t active_casualty_count = 0U;
  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    active_casualty_count = static_cast<std::size_t>(
        std::count_if(casualties_comp->entries.begin(),
                      casualties_comp->entries.end(),
                      [total_layout_count, visible_count](const auto& entry) {
                        return entry.slot_index < total_layout_count &&
                               entry.slot_index >= visible_count;
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
  if (unit_comp != nullptr) {
    switch (unit_comp->nation_id) {
    case Game::Systems::NationID::Carthage:
    case Game::Systems::NationID::IronSepulcher:
      nation = Nation::Carthage;
      break;
    case Game::Systems::NationID::RomanRepublic:
    default:
      break;
    }
  }

  UnitCategory category =
      is_mounted_spawn ? UnitCategory::Cavalry : UnitCategory::Infantry;
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
  std::vector<SoldierLayout> soldier_layouts;
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
        soldier_layouts = entry.soldiers;
        entry.frame_number = frame_index;
        loaded_cached_layouts = true;
      }
    }

    if (!loaded_cached_layouts) {
      soldier_layouts.reserve(static_cast<std::size_t>(total_layout_count));
      for (int idx = 0; idx < total_layout_count; ++idx) {
        SoldierLayoutInputs inputs{};
        inputs.idx = idx;
        inputs.row = idx / cols;
        inputs.col = idx % cols;
        inputs.rows = rows;
        inputs.cols = cols;
        inputs.formation_spacing = formation.spacing;
        inputs.seed = seed;
        inputs.force_single_soldier = ctx.force_single_soldier;
        inputs.melee_attack = anim.is_attacking && anim.is_melee;
        inputs.animation_time = anim.time;
        soldier_layouts.push_back(build_soldier_layout(*formation_calculator, inputs));
      }

      if (layout_cache_comp != nullptr) {
        layout_cache_comp->soldiers = soldier_layouts;
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
  if (ctx.entity != nullptr) {
    if (auto* attack_target =
            ctx.entity->get_component<Engine::Core::AttackTargetComponent>();
        attack_target != nullptr && attack_target->target_id > 0U) {
      unit_attack_target_id = attack_target->target_id;
      if (ctx.world != nullptr) {
        if (auto* target_entity = ctx.world->get_entity(attack_target->target_id);
            target_entity != nullptr &&
            !target_entity->has_component<Engine::Core::PendingRemovalComponent>()) {
          if (auto* death_anim =
                  target_entity->get_component<Engine::Core::DeathAnimationComponent>();
              death_anim == nullptr ||
              death_anim->state == Engine::Core::DeathSequenceState::Dying) {
            unit_attack_target_alive = (death_anim == nullptr);
          }
        }
      }
    }
  }
  auto record_soldier_debug = [&](int idx,
                                  const AnimationInputs&,
                                  const AnimationInputs& resolved_anim,
                                  float attack_phase,
                                  Render::Creature::AnimationStateId animation_state,
                                  HumanoidLOD lod,
                                  Render::Profiling::SoldierCullReason cull_reason,
                                  bool transient_recovery_override) {
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
    sample.is_in_melee_lock = resolved_anim.is_in_melee_lock;
    sample.transient_recovery_override = transient_recovery_override;
    sample.locomotion_state = resolved_anim.movement_state;
    sample.animation_state = animation_state;
    sample.lod = static_cast<std::uint8_t>(lod);
    sample.cull_reason = cull_reason;
    Render::Profiling::CombatAnimationDiagnostics::instance().record_soldier_sample(
        ctx.entity->get_id(), sample);
  };

  auto append_prepared_soldier = [&](int idx, const AnimationInputs& soldier_anim) {
    SoldierLayout const& layout = soldier_layouts[static_cast<std::size_t>(idx)];
    AnimationInputs soldier_render_anim = soldier_anim;
    if (!is_mounted_spawn && guard_pose_amount(soldier_render_anim) > 0.0F &&
        soldier_render_anim.shield_formation_pose == ShieldFormationPose::None) {
      auto const visual_spec = owner.visual_spec();
      int const row = idx / cols;
      int const col = idx % cols;
      soldier_render_anim.shield_formation_pose =
          shared_guard_shield_pose(unit_comp,
                                   visual_spec,
                                   formation_mode,
                                   guard_mode_comp,
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

    auto const hit_reaction_transform =
        Animation::resolve_humanoid_hit_reaction_transform({
            .active = soldier_render_anim.is_hit_reacting,
            .intensity = soldier_render_anim.hit_reaction_intensity,
            .recoil_x = soldier_render_anim.hit_recoil_x,
            .recoil_z = soldier_render_anim.hit_recoil_z,
            .inst_seed = inst_seed,
        });
    float const hit_recoil_len =
        std::sqrt(hit_reaction_transform.recoil_x * hit_reaction_transform.recoil_x +
                  hit_reaction_transform.recoil_z * hit_reaction_transform.recoil_z);

    if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + applied_yaw_offset;
      QMatrix4x4 m = k_identity_matrix;
      m.translate(transform_comp->position.x,
                  transform_comp->position.y,
                  transform_comp->position.z);
      if (hit_recoil_len > 0.0001F) {
        m.translate(
            hit_reaction_transform.recoil_x, 0.0F, hit_reaction_transform.recoil_z);
        if (hit_reaction_transform.tilt_degrees > 0.01F) {

          m.rotate(hit_reaction_transform.tilt_degrees,
                   hit_reaction_transform.recoil_z / hit_recoil_len,
                   0.0F,
                   -hit_reaction_transform.recoil_x / hit_recoil_len);
        }
      }
      m.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      m.scale(
          transform_comp->scale.x, transform_comp->scale.y, transform_comp->scale.z);
      if (hit_reaction_transform.squash > 0.0001F) {

        m.scale(1.0F, 1.0F - hit_reaction_transform.squash, 1.0F);
      }
      m.translate(offset_x, 0.0F, offset_z);
      inst_model = m;
    } else {
      inst_model = ctx.model;
      inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      inst_model.translate(offset_x, 0.0F, offset_z);
    }

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

    auto const commander_attack = preparation_mode.attack_pose(soldier_render_anim);
    std::uint8_t base_attack_variant = soldier_render_anim.attack_variant;
    if (ctx.has_attack_variant_override) {
      base_attack_variant = ctx.attack_variant_override;
    } else if (commander_attack.has_style) {
      base_attack_variant = commander_attack.style;
    }

    Render::Creature::CombatLaneInputs lane_inputs{};
    lane_inputs.unit_seed = seed;
    lane_inputs.soldier_seed = inst_seed;
    lane_inputs.row = static_cast<int>(layout.row_index);
    lane_inputs.col = static_cast<int>(layout.col_index);
    lane_inputs.rows = rows;
    lane_inputs.cols = cols;
    lane_inputs.health_ratio = unit_health_ratio;
    lane_inputs.local_enemy_pressure = local_enemy_pressure;
    lane_inputs.force_single_soldier = ctx.force_single_soldier;
    lane_inputs.is_melee = soldier_render_anim.is_melee;
    lane_inputs.is_mounted = is_mounted_spawn;
    lane_inputs.attack_family = soldier_render_anim.attack_family;

    Render::Creature::SoldierCombatLaneState const lane_snapshot =
        (cached_lane_state != nullptr) ? *cached_lane_state
                                       : Render::Creature::SoldierCombatLaneState{};
    auto const lane_resolution =
        Render::Creature::resolve_soldier_combat_lane(lane_snapshot, lane_inputs);
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
    raw_combat.attack_requested = soldier_render_anim.is_attacking;
    raw_combat.is_melee = soldier_render_anim.is_melee;
    raw_combat.is_mounted = is_mounted_spawn;
    raw_combat.is_casting = soldier_render_anim.is_casting;
    raw_combat.finisher_attack = soldier_render_anim.finisher_attack;
    raw_combat.amplified_attack = commander_attack.amplified;
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

    auto const combat_resolution = Render::Creature::resolve_combat_visual_state(
        previous_combat_visual, raw_combat, lane_resolution.profile);
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
    anim_ctx.amplified_attack = anim_ctx.inputs.combat_visual.amplified_attack;
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

    HumanoidPose pose{};
    bool const requires_runtime_pose =
        RCP::pass_intent_from_ctx(inst_ctx) == RCP::RenderPassIntent::Shadow;
    if (requires_runtime_pose) {
      HumanoidRendererBase::compute_locomotion_pose(
          inst_seed, anim.time, locomotion_state.gait, variation, pose);

      const float hold_kneel_depth = owner.get_hold_kneel_depth();
      float const effective_kneel =
          hold_transition_amount(anim_ctx.inputs) * hold_kneel_depth;
      if (effective_kneel > 1e-4F) {
        HumanoidPoseController kneel_ctrl(pose, anim_ctx);
        kneel_ctrl.kneel(effective_kneel);
      }
      float const guard_amount = guard_pose_amount(anim_ctx.inputs);
      if (guard_amount > 0.0F && !render_has_locomotion &&
          !anim_ctx.inputs.is_attacking) {
        HumanoidPoseController guard_ctrl(pose, anim_ctx);
        guard_ctrl.guard_sword_and_shield_formation(
            anim_ctx.inputs.shield_formation_pose, guard_amount);
      }
      if (anim_ctx.inputs.is_constructing) {
        HumanoidPoseController construct_ctrl(pose, anim_ctx);
        float const construct_phase = RCP::humanoid_phase_for_anim(anim_ctx);
        switch (construction_role) {
        case ConstructionRole::Saw:
          construct_ctrl.construction_saw(construct_phase);
          break;
        case ConstructionRole::Chisel:
          construct_ctrl.construction_chisel(construct_phase, false);
          break;
        case ConstructionRole::KneelingChisel:
          construct_ctrl.kneel(0.84F);
          construct_ctrl.construction_chisel(construct_phase, true);
          break;
        case ConstructionRole::Hammer:
        case ConstructionRole::None:
          construct_ctrl.sword_slash_variant(
              construct_phase,
              RCP::humanoid_clip_variant_for_anim(visual_spec.archetype_id, anim_ctx));
          break;
        }
      }

      apply_spec_pose_layer(visual_spec, inst_ctx, anim_ctx, variant, inst_seed, pose);
      apply_combat_micro_variation(anim_ctx,
                                   inst_seed,
                                   !ctx.force_single_soldier && total_layout_count > 1,
                                   pose);
    }
    bool const world_already_grounded = ctx.skip_ground_offset || requires_runtime_pose;
    if (!ctx.skip_ground_offset && requires_runtime_pose) {
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
      RCP::ground_model_contact_to_surface(inst_ctx.model,
                                           grounded_contact_y,
                                           combined_height_scale,
                                           entity_ground_offset);
    } else if (!world_already_grounded) {
      RCP::ground_model_contact_to_surface(
          inst_ctx.model, 0.0F, combined_height_scale, entity_ground_offset);
    }
    if (commander_jump.height_offset > 0.0F) {
      RCP::set_model_world_y(inst_ctx.model,
                             RCP::model_world_origin(inst_ctx.model).y() +
                                 commander_jump.height_offset);
    }
    anim_ctx.instance_position = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    QVector3D const soldier_world_pos = anim_ctx.instance_position;

    constexpr float k_soldier_cull_radius = 0.6F;
    if (ctx.camera != nullptr &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      record_soldier_debug(
          idx,
          soldier_render_anim,
          anim_ctx.inputs,
          anim_ctx.attack_phase,
          Render::Creature::resolve_pose(anim_ctx.inputs).animation_state,
          HumanoidLOD::Billboard,
          Render::Profiling::SoldierCullReason::Frustum,
          false);
      return;
    }

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
          false);
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
                         transient_recovery_override);

    RCP::HumanoidShadowStateInputs shadow_inputs{};
    shadow_inputs.ctx = &inst_ctx;
    shadow_inputs.graph = &graph_output;
    shadow_inputs.unit = unit_comp;
    shadow_inputs.soldier_world_pos = soldier_world_pos;
    shadow_inputs.lod = soldier_lod;
    shadow_inputs.camera_distance = lod_state.camera_distance;
    shadow_inputs.mounted = is_mounted_spawn;
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

  for (int idx = 0; idx < visible_count; ++idx) {
    append_prepared_soldier(idx, anim);
  }

  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    for (const auto& entry : casualties_comp->entries) {
      if (entry.slot_index >= total_layout_count || entry.slot_index < visible_count) {
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
      append_prepared_soldier(static_cast<int>(entry.slot_index), casualty_anim);
    }
  }
}

} // namespace Render::Humanoid
