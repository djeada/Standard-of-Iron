#include "prepare_internal.h"

namespace Render::Humanoid {

auto wrap_phase(float phase) noexcept -> float {
  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto smoothing_alpha(float dt, float tau) noexcept -> float {
  if (dt <= 1.0e-4F) {
    return 1.0F;
  }
  return 1.0F - std::exp(-dt / std::max(tau, 1.0e-4F));
}

auto smooth_towards(float current,
                    float target,
                    float dt,
                    float tau) noexcept -> float {
  return current + (target - current) * smoothing_alpha(dt, tau);
}

auto lerp(float a, float b, float t) noexcept -> float {
  return a + (b - a) * t;
}

void reset_humanoid_locomotion_state(
    Render::Creature::HumanoidAnimationStateComponent& state) {
  state.locomotion_last_sample_time = 0.0F;
  state.locomotion_phase_bias = 0.0F;
  state.locomotion_cycle_time = 0.0F;
  state.locomotion_phase = 0.0F;
  state.filtered_speed = 0.0F;
  state.filtered_acceleration = 0.0F;
  state.filtered_turn = 0.0F;
  state.locomotion_blend = 0.0F;
  state.run_blend = 0.0F;
  state.locomotion_state = HumanoidMotionState::Idle;
  state.locomotion_initialized = false;
  state.combat_visual = {};
}

void sync_combat_visual_inputs(
    AnimationInputs& inputs,
    const Render::Creature::CombatVisualResolvedState& visual) {
  inputs.combat_visual = visual;
  if (!visual.authoritative) {
    return;
  }

  inputs.is_attacking = visual.active;
  inputs.is_melee = visual.is_melee;
  inputs.is_mounted = visual.is_mounted;
  inputs.is_casting = visual.is_casting;
  inputs.finisher_attack = visual.finisher_attack;
  inputs.attack_family = visual.attack_family;
  inputs.attack_variant = visual.attack_variant;

  if (!visual.active) {
    if (visual.lane == Render::Creature::SoldierCombatLane::ShieldBrace) {
      inputs.is_guarding = true;
    } else if (visual.lane == Render::Creature::SoldierCombatLane::StepOut) {
      inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
    }
    inputs.combat_phase = Render::GL::CombatAnimPhase::Idle;
    inputs.combat_phase_progress = 0.0F;
    return;
  }

  auto const scrubbed = Render::Profiling::scrubbed_combat_phase_from_attack_phase(
      visual.attack_phase, visual.amplified_attack, visual.finisher_attack);
  inputs.combat_phase = scrubbed.phase;
  inputs.combat_phase_progress = scrubbed.progress;
}

void apply_combat_micro_variation(const HumanoidAnimationContext& anim_ctx,
                                  std::uint32_t inst_seed,
                                  bool multi_soldier_unit,
                                  HumanoidPose& pose) {
  if (!multi_soldier_unit || !anim_ctx.inputs.combat_visual.authoritative ||
      anim_ctx.inputs.is_dying || anim_ctx.inputs.is_dead) {
    return;
  }

  auto const lane = anim_ctx.inputs.combat_visual.lane;
  float const time = anim_ctx.inputs.time;
  float const attack_phase = std::clamp(anim_ctx.attack_phase, 0.0F, 1.0F);
  float const lane_sign = ((inst_seed >> 7U) & 1U) == 0U ? -1.0F : 1.0F;
  float const breath =
      std::sin(time * 5.3F + static_cast<float>(inst_seed & 31U)) * 0.004F;
  float const pressure = anim_ctx.inputs.is_attacking ? 1.0F : 0.45F;
  float torso_lean = 0.0F;
  float lateral_tilt = 0.0F;
  float shoulder_delay = 0.0F;
  float wrist_angle = 0.0F;
  float foot_adjust = 0.0F;
  float shield_reaction = 0.0F;
  float head_tracking = 0.0F;
  float impact_stagger = 0.0F;

  switch (lane) {
  case Render::Creature::SoldierCombatLane::LeadStrike:
    torso_lean = 0.030F * pressure;
    shoulder_delay = 0.018F;
    wrist_angle = 0.014F;
    foot_adjust = 0.022F;
    head_tracking = 0.010F;
    break;
  case Render::Creature::SoldierCombatLane::SupportStrike:
    torso_lean = 0.018F * pressure;
    shoulder_delay = -0.010F;
    wrist_angle = 0.010F;
    foot_adjust = 0.014F;
    head_tracking = 0.008F;
    break;
  case Render::Creature::SoldierCombatLane::ShieldBrace:
    lateral_tilt = -0.018F * lane_sign;
    shield_reaction = 0.020F;
    foot_adjust = -0.010F;
    head_tracking = -0.006F;
    break;
  case Render::Creature::SoldierCombatLane::StepIn:
    torso_lean = 0.016F;
    foot_adjust = 0.030F;
    head_tracking = 0.006F;
    break;
  case Render::Creature::SoldierCombatLane::StepOut:
    torso_lean = -0.012F;
    foot_adjust = -0.028F;
    lateral_tilt = 0.010F * lane_sign;
    break;
  case Render::Creature::SoldierCombatLane::IdleReady:
    lateral_tilt = 0.008F * lane_sign;
    head_tracking = 0.005F;
    break;
  case Render::Creature::SoldierCombatLane::RangedReload:
    torso_lean = -0.014F;
    shoulder_delay = -0.012F;
    wrist_angle = -0.010F;
    foot_adjust = -0.012F;
    break;
  case Render::Creature::SoldierCombatLane::None:
    break;
  }

  if (anim_ctx.inputs.combat_phase == Render::GL::CombatAnimPhase::Impact ||
      (attack_phase >= 0.50F && attack_phase <= 0.68F)) {
    impact_stagger = 0.016F * pressure;
  }

  pose.neck_base.setY(pose.neck_base.y() + breath);
  pose.head_pos.setY(pose.head_pos.y() + breath * 1.4F);
  pose.shoulder_l.setY(pose.shoulder_l.y() + breath);
  pose.shoulder_r.setY(pose.shoulder_r.y() + breath * 0.9F);

  pose.shoulder_l.setZ(pose.shoulder_l.z() - torso_lean + impact_stagger * 0.3F);
  pose.shoulder_r.setZ(pose.shoulder_r.z() - torso_lean + impact_stagger * 0.3F);
  pose.head_pos.setZ(pose.head_pos.z() - torso_lean * 0.55F - impact_stagger);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() + torso_lean * 0.30F +
                       impact_stagger * 0.7F);

  pose.shoulder_l.setX(pose.shoulder_l.x() + lateral_tilt + head_tracking);
  pose.shoulder_r.setX(pose.shoulder_r.x() + lateral_tilt + head_tracking);
  pose.head_pos.setX(pose.head_pos.x() + lateral_tilt * 0.55F +
                     head_tracking * lane_sign);
  pose.neck_base.setX(pose.neck_base.x() + lateral_tilt * 0.35F);

  pose.hand_r.setZ(pose.hand_r.z() + shoulder_delay + wrist_angle);
  pose.elbow_r.setZ(pose.elbow_r.z() + shoulder_delay * 0.6F);
  pose.hand_l.setZ(pose.hand_l.z() - shield_reaction);
  pose.elbow_l.setZ(pose.elbow_l.z() - shield_reaction * 0.6F);

  pose.foot_l.setZ(pose.foot_l.z() + foot_adjust);
  pose.knee_l.setZ(pose.knee_l.z() + foot_adjust * 0.6F);
  pose.foot_r.setZ(pose.foot_r.z() - foot_adjust * 0.5F);
  pose.knee_r.setZ(pose.knee_r.z() - foot_adjust * 0.3F);
}

auto reference_cycle_time_for_motion_state(
    HumanoidMotionState state, const VariationParams& variation) noexcept -> float {
  if (state == HumanoidMotionState::Run) {
    return 0.56F / std::max(0.1F, variation.walk_speed_mult);
  }
  if (state == HumanoidMotionState::Walk) {
    return 0.92F / std::max(0.1F, variation.walk_speed_mult);
  }
  return k_humanoid_idle_cycle_time;
}

auto walk_cycle_time_for_speed(float normalized_speed,
                               const VariationParams& variation) noexcept -> float {
  return lerp(1.08F, 0.90F, std::clamp(normalized_speed, 0.0F, 1.0F)) /
         std::max(0.1F, variation.walk_speed_mult);
}

auto run_cycle_time_for_speed(float normalized_speed,
                              const VariationParams& variation) noexcept -> float {
  return lerp(0.62F, 0.52F, std::clamp(normalized_speed, 0.0F, 1.0F)) /
         std::max(0.1F, variation.walk_speed_mult);
}

auto flattened_or(const QVector3D& v, const QVector3D& fallback) noexcept -> QVector3D {
  QVector3D flat(v.x(), 0.0F, v.z());
  if (flat.lengthSquared() <= 1.0e-6F) {
    flat = fallback;
  }
  if (flat.lengthSquared() <= 1.0e-6F) {
    flat = QVector3D(0.0F, 0.0F, 1.0F);
  }
  flat.normalize();
  return flat;
}

auto resolve_visual_locomotion_sample(
    const Render::GL::VisualMovementState& movement_state,
    const QVector3D& entity_forward) noexcept -> VisualLocomotionSample {
  VisualLocomotionSample sample{};
  sample.direction = entity_forward;
  if (Render::Creature::is_moving_animation(movement_state.movement_state)) {
    sample.speed = movement_state.speed_hint;
    sample.direction = movement_state.locomotion_direction;
    sample.has_movement_target = movement_state.has_movement_target;
    sample.movement_target = movement_state.movement_target;
  }

  return sample;
}

auto signed_turn_amount(const QVector3D& entity_forward,
                        const QVector3D& locomotion_direction) noexcept -> float {
  QVector3D const forward = flattened_or(entity_forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D const motion = flattened_or(locomotion_direction, forward);
  return std::clamp(QVector3D::crossProduct(forward, motion).y(), -1.0F, 1.0F);
}

auto build_locomotion_targets(const HumanoidLocomotionState& state,
                              const HumanoidLocomotionInputs& inputs) noexcept
    -> LocomotionTargets {
  LocomotionTargets targets{};
  bool const has_locomotion =
      Render::Creature::is_moving_animation(inputs.anim.movement_state);
  float const reference_speed = (state.gait.state == HumanoidMotionState::Run)
                                    ? k_reference_run_speed
                                    : k_reference_walk_speed;
  targets.normalized_speed =
      (has_locomotion && reference_speed > 0.0001F)
          ? std::clamp(state.gait.speed / reference_speed, 0.0F, 1.0F)
          : 0.0F;
  targets.locomotion_blend =
      has_locomotion ? std::clamp(targets.normalized_speed * 1.10F, 0.0F, 1.0F) : 0.0F;
  targets.run_blend = has_locomotion
                          ? std::clamp((state.gait.state == HumanoidMotionState::Run)
                                           ? 0.70F + targets.normalized_speed * 0.30F
                                           : 0.0F,
                                       0.0F,
                                       1.0F)
                          : 0.0F;
  targets.turn_amount = has_locomotion ? signed_turn_amount(inputs.entity_forward,
                                                            inputs.locomotion_direction)
                                       : 0.0F;
  if (has_locomotion) {
    float const walk_cycle_time =
        walk_cycle_time_for_speed(targets.normalized_speed, inputs.variation);
    float const run_cycle_time =
        run_cycle_time_for_speed(targets.normalized_speed, inputs.variation);
    targets.cycle_time = lerp(walk_cycle_time, run_cycle_time, targets.run_blend);
  } else {
    targets.cycle_time = k_humanoid_idle_cycle_time;
  }
  targets.base_phase = wrap_phase((inputs.animation_time + inputs.phase_offset) /
                                  std::max(0.001F, targets.cycle_time));
  return targets;
}

void apply_persistent_locomotion_state(HumanoidLocomotionState& state,
                                       const HumanoidLocomotionInputs& inputs,
                                       const LocomotionTargets& targets) {
  auto* persistent = inputs.persistent_state;
  if (persistent == nullptr) {
    return;
  }

  if (persistent->locomotion_initialized &&
      inputs.animation_time < persistent->locomotion_last_sample_time &&
      inputs.allow_persistent_update) {
    reset_humanoid_locomotion_state(*persistent);
  }

  float delta_time = 0.0F;
  if (persistent->locomotion_initialized) {
    delta_time =
        std::max(0.0F, inputs.animation_time - persistent->locomotion_last_sample_time);
  }

  if (persistent->locomotion_initialized &&
      Render::Creature::is_moving_animation(inputs.anim.movement_state)) {
    float const previous_cycle_time = (persistent->locomotion_cycle_time > 1.0e-4F)
                                          ? persistent->locomotion_cycle_time
                                          : targets.cycle_time;
    state.gait.cycle_time = smooth_towards(
        previous_cycle_time, targets.cycle_time, delta_time, k_cadence_blend_tau);
    state.gait.cycle_phase =
        wrap_phase(persistent->locomotion_phase +
                   delta_time / std::max(0.001F, state.gait.cycle_time));
  } else {
    state.gait.cycle_time = targets.cycle_time;
    state.gait.cycle_phase = targets.base_phase;
  }

  if (persistent->locomotion_initialized) {
    float const previous_filtered_speed = persistent->filtered_speed;
    state.gait.normalized_speed = smooth_towards(previous_filtered_speed,
                                                 targets.normalized_speed,
                                                 delta_time,
                                                 k_locomotion_blend_tau);
    state.gait.locomotion_blend = smooth_towards(persistent->locomotion_blend,
                                                 targets.locomotion_blend,
                                                 delta_time,
                                                 k_locomotion_blend_tau);
    state.gait.run_blend = smooth_towards(
        persistent->run_blend, targets.run_blend, delta_time, k_run_blend_tau);
    state.gait.turn_amount = smooth_towards(
        persistent->filtered_turn, targets.turn_amount, delta_time, k_turn_blend_tau);

    float instant_acceleration = 0.0F;
    if (delta_time > 1.0e-4F) {
      instant_acceleration =
          (state.gait.normalized_speed - previous_filtered_speed) / delta_time;
    }
    state.gait.acceleration =
        smooth_towards(persistent->filtered_acceleration,
                       std::clamp(instant_acceleration, -4.0F, 4.0F),
                       delta_time,
                       k_acceleration_blend_tau);
  }

  if (!inputs.allow_persistent_update) {
    return;
  }

  persistent->locomotion_initialized = true;
  persistent->locomotion_last_sample_time = inputs.animation_time;
  persistent->locomotion_phase = state.gait.cycle_phase;
  persistent->locomotion_phase_bias = state.gait.cycle_phase - targets.base_phase;
  persistent->locomotion_cycle_time = state.gait.cycle_time;
  persistent->filtered_speed = state.gait.normalized_speed;
  persistent->filtered_acceleration = state.gait.acceleration;
  persistent->filtered_turn = state.gait.turn_amount;
  persistent->locomotion_blend = state.gait.locomotion_blend;
  persistent->run_blend = state.gait.run_blend;
  persistent->locomotion_state = state.gait.state;
}

auto build_soldier_layout(const IFormationCalculator& formation_calculator,
                          const SoldierLayoutInputs& inputs) -> SoldierLayout {
  SoldierLayout layout{};
  layout.row_index = static_cast<std::uint8_t>(std::clamp(inputs.row, 0, 255));
  layout.col_index = static_cast<std::uint8_t>(std::clamp(inputs.col, 0, 255));
  layout.rank_band = static_cast<std::uint8_t>(
      (inputs.force_single_soldier || inputs.rows <= 1)
          ? 0
          : ((inputs.row <= 0) ? 0 : ((inputs.row + 1 >= inputs.rows) ? 2 : 1)));
  layout.inst_seed = inputs.seed ^ std::uint32_t(inputs.idx * 9176U);

  std::uint32_t rng_state = layout.inst_seed;
  if (!inputs.force_single_soldier) {
    FormationOffset const formation_offset =
        formation_calculator.calculate_offset(inputs.idx,
                                              inputs.row,
                                              inputs.col,
                                              inputs.rows,
                                              inputs.cols,
                                              inputs.formation_spacing,
                                              inputs.seed);
    layout.offset_x = formation_offset.offset_x;
    layout.offset_z = formation_offset.offset_z;
    layout.vertical_jitter =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 0.03F;
    layout.yaw_offset =
        formation_offset.yaw_offset +
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 5.0F;
    layout.phase_offset = Render::GL::structured_layout_phase_offset(
        inputs.row, inputs.col, inputs.rows, inputs.cols, layout.inst_seed);
  }

  if (inputs.melee_attack) {
    float const combat_jitter_x =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) *
        inputs.formation_spacing * 0.4F;
    float const combat_jitter_z =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) *
        inputs.formation_spacing * 0.3F;
    float const sway_time = inputs.animation_time + layout.phase_offset * 2.0F;
    float const sway_x = std::sin(sway_time * 1.5F) * 0.05F;
    float const sway_z = std::cos(sway_time * 1.2F) * 0.04F;
    float const combat_yaw =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 15.0F;
    layout.offset_x += combat_jitter_x + sway_x;
    layout.offset_z += combat_jitter_z + sway_z;
    layout.yaw_offset += combat_yaw;
  }

  return layout;
}

auto build_humanoid_locomotion_state(const HumanoidLocomotionInputs& inputs)
    -> HumanoidLocomotionState {
  HumanoidLocomotionState state{};
  auto const gait_state = Render::Creature::resolve_pose(inputs.anim).motion_state;
  state.move_speed = inputs.move_speed;
  state.has_movement_target = inputs.has_movement_target;
  state.locomotion_direction = inputs.locomotion_direction;
  state.locomotion_velocity = inputs.locomotion_direction * inputs.move_speed;
  state.movement_target = inputs.movement_target;

  state.gait.state = gait_state;
  state.gait.speed = state.move_speed;
  state.gait.velocity = state.locomotion_velocity;
  state.gait.has_target = state.has_movement_target;
  state.gait.is_airborne = false;

  LocomotionTargets const targets = build_locomotion_targets(state, inputs);
  state.gait.cycle_time = targets.cycle_time;
  state.gait.cycle_phase = targets.base_phase;
  state.gait.normalized_speed = targets.normalized_speed;
  state.gait.locomotion_blend = targets.locomotion_blend;
  state.gait.run_blend = targets.run_blend;
  state.gait.turn_amount = targets.turn_amount;
  state.gait.acceleration = 0.0F;

  apply_persistent_locomotion_state(state, inputs, targets);

  state.gait.stride_distance =
      Render::Creature::is_moving_animation(inputs.anim.movement_state)
          ? state.gait.speed * state.gait.cycle_time *
                std::max(0.0F, state.gait.locomotion_blend)
          : 0.0F;

  return state;
}

} // namespace Render::Humanoid
