#include "render/creature/humanoid/runtime/instance_prepare_stages.h"

namespace Render::Humanoid {

namespace instance_prepare_detail {

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

} // namespace instance_prepare_detail

} // namespace Render::Humanoid
