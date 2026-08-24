#include <algorithm>
#include <cmath>
#include <cstdint>

#include "animation/locomotion_manifest.h"
#include "animation/micro_variation_manifest.h"
#include "render/creature/animation_core_bridge.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/pose_intent.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_constants.h"
#include "render/humanoid/runtime/humanoid_runtime_types.h"

namespace Render::Humanoid {

void reset_humanoid_locomotion_state(
    Render::Creature::HumanoidAnimationStateComponent& state) {
  state.locomotion = {};
  state.construction_transition = {};
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
  switch (visual.attack_family) {
  case Animation::CombatAttackFamily::Sword:
    inputs.attack_family = Engine::Core::CombatAttackFamily::Sword;
    break;
  case Animation::CombatAttackFamily::Spear:
    inputs.attack_family = Engine::Core::CombatAttackFamily::Spear;
    break;
  case Animation::CombatAttackFamily::Bow:
    inputs.attack_family = Engine::Core::CombatAttackFamily::Bow;
    break;
  case Animation::CombatAttackFamily::None:
    inputs.attack_family = Engine::Core::CombatAttackFamily::None;
    break;
  }
  inputs.attack_variant = visual.attack_variant;

  if (!visual.active) {
    if (visual.lane == Render::Creature::SoldierCombatLane::ShieldBrace) {
      inputs.is_guarding = true;
    }
    inputs.combat_phase = Render::GL::CombatAnimPhase::Idle;
    inputs.combat_phase_progress = 0.0F;
    return;
  }

  auto const scrubbed = Animation::scrubbed_combat_phase_from_attack_phase(
      visual.attack_phase, visual.amplified_attack, visual.finisher_attack);
  inputs.combat_phase = scrubbed.phase;
  inputs.combat_phase_progress = scrubbed.progress;
}

void apply_combat_micro_variation(const HumanoidAnimationContext& anim_ctx,
                                  std::uint32_t inst_seed,
                                  bool multi_soldier_unit,
                                  HumanoidPose& pose) {
  auto const variation = Animation::resolve_humanoid_combat_micro_variation({
      .multi_soldier_unit = multi_soldier_unit,
      .authoritative_combat = anim_ctx.inputs.combat_visual.authoritative,
      .is_dying = anim_ctx.inputs.is_dying,
      .is_dead = anim_ctx.inputs.is_dead,
      .lane = anim_ctx.inputs.combat_visual.lane,
      .combat_phase = anim_ctx.inputs.combat_phase,
      .is_attacking = anim_ctx.inputs.is_attacking,
      .attack_phase = anim_ctx.attack_phase,
      .sample_time = anim_ctx.inputs.time,
      .inst_seed = inst_seed,
  });
  if (!variation.active) {
    return;
  }

  pose.neck_base.setY(pose.neck_base.y() + variation.breath);
  pose.head_pos.setY(pose.head_pos.y() + variation.breath * 1.4F);
  pose.shoulder_l.setY(pose.shoulder_l.y() + variation.breath);
  pose.shoulder_r.setY(pose.shoulder_r.y() + variation.breath * 0.9F);

  pose.shoulder_l.setZ(pose.shoulder_l.z() - variation.torso_lean +
                       variation.impact_stagger * 0.3F);
  pose.shoulder_r.setZ(pose.shoulder_r.z() - variation.torso_lean +
                       variation.impact_stagger * 0.3F);
  pose.head_pos.setZ(pose.head_pos.z() - variation.torso_lean * 0.55F -
                     variation.impact_stagger);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() + variation.torso_lean * 0.30F +
                       variation.impact_stagger * 0.7F);

  pose.shoulder_l.setX(pose.shoulder_l.x() + variation.lateral_tilt +
                       variation.head_tracking);
  pose.shoulder_r.setX(pose.shoulder_r.x() + variation.lateral_tilt +
                       variation.head_tracking);
  pose.head_pos.setX(pose.head_pos.x() + variation.lateral_tilt * 0.55F +
                     variation.head_tracking * variation.lane_sign);
  pose.neck_base.setX(pose.neck_base.x() + variation.lateral_tilt * 0.35F);

  pose.hand_r.setZ(pose.hand_r.z() + variation.shoulder_delay + variation.wrist_angle);
  pose.elbow_r.setZ(pose.elbow_r.z() + variation.shoulder_delay * 0.6F);
  pose.hand_l.setZ(pose.hand_l.z() - variation.shield_reaction);
  pose.elbow_l.setZ(pose.elbow_l.z() - variation.shield_reaction * 0.6F);

  pose.foot_l.setZ(pose.foot_l.z() + variation.foot_adjust);
  pose.knee_l.setZ(pose.knee_l.z() + variation.foot_adjust * 0.6F);
  pose.foot_r.setZ(pose.foot_r.z() - variation.foot_adjust * 0.5F);
  pose.knee_r.setZ(pose.knee_r.z() - variation.foot_adjust * 0.3F);
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

namespace {

[[nodiscard]] auto humanoid_locomotion_tuning(const VariationParams& variation) noexcept
    -> Animation::HumanoidLocomotionTuning {
  return {
      .reference_walk_speed = k_reference_walk_speed,
      .reference_run_speed = k_reference_run_speed,
      .walk_speed_multiplier = variation.walk_speed_mult,
      .idle_cycle_time = k_humanoid_idle_cycle_time,
      .locomotion_blend_tau = k_locomotion_blend_tau,
      .cadence_blend_tau = k_cadence_blend_tau,
      .run_blend_tau = k_run_blend_tau,
      .turn_blend_tau = k_turn_blend_tau,
      .acceleration_blend_tau = k_acceleration_blend_tau,
  };
}

} // namespace

auto build_humanoid_locomotion_state(const HumanoidLocomotionInputs& inputs)
    -> HumanoidLocomotionState {
  HumanoidLocomotionState state{};
  auto const gait_state = Render::Creature::resolve_pose(inputs.anim).motion_state;
  auto const persistent = inputs.persistent_state != nullptr
                              ? inputs.persistent_state->locomotion
                              : Animation::HumanoidLocomotionPersistentState{};
  auto const sample = Animation::resolve_humanoid_locomotion_sample({
      .movement_state = inputs.anim.movement_state,
      .motion_state = gait_state,
      .speed = inputs.move_speed,
      .entity_forward_x = inputs.entity_forward.x(),
      .entity_forward_z = inputs.entity_forward.z(),
      .locomotion_direction_x = inputs.locomotion_direction.x(),
      .locomotion_direction_z = inputs.locomotion_direction.z(),
      .sample_time = inputs.animation_time,
      .phase_offset = inputs.individuality.gait_phase_offset,
      .cadence_scale = inputs.individuality.cadence_scale,
      .tuning = humanoid_locomotion_tuning(inputs.variation),
      .has_persistent_state = inputs.persistent_state != nullptr,
      .previous = persistent,
      .allow_persistent_update = inputs.allow_persistent_update,
  });
  state.move_speed = inputs.move_speed;
  state.has_movement_target = inputs.has_movement_target;
  state.locomotion_direction = inputs.locomotion_direction;
  state.locomotion_velocity = inputs.locomotion_direction * inputs.move_speed;
  state.movement_target = inputs.movement_target;

  state.gait.state = sample.state;
  state.gait.speed = state.move_speed;
  state.gait.velocity = state.locomotion_velocity;
  state.gait.has_target = state.has_movement_target;
  state.gait.is_airborne = false;
  state.gait.cycle_time = sample.cycle_time;
  state.gait.cycle_phase = sample.cycle_phase;
  state.gait.normalized_speed = sample.normalized_speed;
  state.gait.stride_distance = sample.stride_distance;
  state.gait.locomotion_blend = sample.locomotion_blend;
  state.gait.run_blend = sample.run_blend;
  state.gait.locomotion_presence = sample.locomotion_presence;
  state.gait.persistent_valid = sample.previous_seen_initialized;
  state.gait.persistent_last_sample_time = sample.previous_seen_presence;
  state.gait.run_presence = sample.run_presence;
  state.gait.turn_amount = sample.turn_amount;
  state.gait.travel_alignment = sample.travel_alignment;
  state.gait.reverse_gait = sample.reverse_gait;
  state.gait.acceleration = sample.acceleration;

  if (sample.write_persistent_state && inputs.persistent_state != nullptr) {
    inputs.persistent_state->locomotion = sample.persistent;
  }

  return state;
}

} // namespace Render::Humanoid
