#include "render/creature/humanoid/runtime/instance_prepare_internal.h"

namespace Render::Humanoid {

namespace instance_prepare_detail {

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

} // namespace instance_prepare_detail

} // namespace Render::Humanoid
