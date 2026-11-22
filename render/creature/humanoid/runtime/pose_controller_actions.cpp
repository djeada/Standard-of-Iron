#include "render/creature/humanoid/runtime/pose_controller_internal.h"

namespace Render::GL {

using namespace pose_controller_detail;

void HumanoidPoseController::aim_bow(float draw_phase) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_bow_draw_pose({
      .draw_phase = draw_phase,
      .jitter_seed = m_anim_ctx.jitter_seed,
      .shoulder_y = 0.5F * (m_pose.shoulder_l.y() + m_pose.shoulder_r.y()),
  });
  apply_bow_draw_body_deltas(m_pose, sample);
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  m_pose.elbow_l = Render::Humanoid::PosePrimitives::solve_arm_ik(
      m_pose.shoulder_l,
      m_pose.hand_l,
      {.upper_arm_len = HP::UPPER_ARM_LEN,
       .fore_arm_len = HP::FORE_ARM_LEN,
       .bend_preference = to_qvec(sample.string_elbow_pole)});
  aim_held_weapon(m_pose, QVector3D(0.0F, 1.0F, 0.0F), k_baked_bow_axis);
}

void HumanoidPoseController::bow_melee_strike(float attack_phase,
                                              std::uint8_t variant) {
  auto sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::BowMeleeStrike,
                                m_anim_ctx,
                                attack_phase,
                                variant,
                                1.0F));
  bool const orients_bow = sample.has_blade_direction;
  sample.has_blade_direction = false;
  apply_weapon_attack_sample(*this, m_pose, sample);
  if (orients_bow) {
    aim_held_weapon(m_pose, to_qvec(sample.blade_direction), k_baked_bow_axis);
  }
}

void HumanoidPoseController::melee_strike(float strike_phase) {
  unarmed_strike(strike_phase, 0U);
}

void HumanoidPoseController::unarmed_strike(float strike_phase, std::uint8_t variant) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::BasicMeleeStrike,
                                m_anim_ctx,
                                strike_phase,
                                variant,
                                1.0F));
  apply_weapon_attack_sample(*this, m_pose, sample);
}

void HumanoidPoseController::grasp_two_handed(const QVector3D& grip_center,
                                              float hand_separation) {
  hand_separation = std::clamp(hand_separation, 0.1F, 0.8F);

  QVector3D const right_axis = compute_right_axis();

  QVector3D const right_hand_pos = grip_center + right_axis * (hand_separation * 0.5F);
  QVector3D const left_hand_pos = grip_center - right_axis * (hand_separation * 0.5F);

  place_hand_at(Side::Right, right_hand_pos);
  place_hand_at(Side::Left, left_hand_pos);
}

void HumanoidPoseController::construction_saw(float work_phase) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = Animation::HumanoidConstructionPoseKind::Saw,
      .work_phase = work_phase,
      .jitter_seed = m_anim_ctx.jitter_seed,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_construction_body_deltas(m_pose, sample);
  grasp_two_handed(to_qvec(sample.grip_center), sample.hand_separation);
}

void HumanoidPoseController::construction_pose(
    Animation::HumanoidConstructionPoseKind kind, float work_phase) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = kind,
      .work_phase = work_phase,
      .jitter_seed = m_anim_ctx.jitter_seed,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_construction_body_deltas(m_pose, sample);
  if (sample.use_two_handed_grip) {
    grasp_two_handed(to_qvec(sample.grip_center), sample.hand_separation);
    return;
  }
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
}

void HumanoidPoseController::construction_hammer(float work_phase) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = Animation::HumanoidConstructionPoseKind::Hammer,
      .work_phase = work_phase,
      .jitter_seed = m_anim_ctx.jitter_seed,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_construction_body_deltas(m_pose, sample);
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
}

void HumanoidPoseController::construction_reap(float work_phase) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = Animation::HumanoidConstructionPoseKind::Reap,
      .work_phase = work_phase,
      .jitter_seed = m_anim_ctx.jitter_seed,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_construction_body_deltas(m_pose, sample);
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
}

void HumanoidPoseController::climb_ladder(float cycle_phase) {
  using HP = HumanProportions;
  float const two_pi = 2.0F * std::numbers::pi_v<float>;
  float const s = std::sin(cycle_phase * two_pi);

  float const left_step = std::max(0.0F, s);
  float const right_step = std::max(0.0F, -s);
  float const ground = HP::GROUND_Y + m_pose.foot_y_offset;
  constexpr float k_rung_lift = 0.30F;
  constexpr float k_ladder_reach = 0.16F;

  m_pose.pelvis_pos += QVector3D(0.0F, -0.04F - 0.03F * std::abs(s), 0.05F);
  lean(QVector3D(0.0F, 0.0F, 1.0F), 1.0F);

  m_pose.foot_l = QVector3D(
      -0.11F, ground + k_rung_lift * left_step, k_ladder_reach + 0.10F * left_step);
  m_pose.foot_r = QVector3D(
      0.11F, ground + k_rung_lift * right_step, k_ladder_reach + 0.10F * right_step);
  m_pose.foot_pitch_l = -0.35F * left_step;
  m_pose.foot_pitch_r = -0.35F * right_step;
  QVector3D const hip_l =
      Render::Humanoid::PosePrimitives::hip_position(m_pose.pelvis_pos, Side::Left);
  QVector3D const hip_r =
      Render::Humanoid::PosePrimitives::hip_position(m_pose.pelvis_pos, Side::Right);
  m_pose.knee_l = solve_knee_ik(Side::Left, hip_l, m_pose.foot_l, 1.0F);
  m_pose.knee_r = solve_knee_ik(Side::Right, hip_r, m_pose.foot_r, 1.0F);

  float const grip_y = HP::SHOULDER_Y + 0.06F;
  constexpr float k_hand_lift = 0.26F;
  place_hand_at(Side::Left,
                QVector3D(-0.19F, grip_y + k_hand_lift * right_step, 0.44F));
  place_hand_at(Side::Right, QVector3D(0.19F, grip_y + k_hand_lift * left_step, 0.44F));
}

void HumanoidPoseController::construction_chisel(float work_phase, bool kneeling) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_construction_pose({
      .kind = kneeling ? Animation::HumanoidConstructionPoseKind::KneelingChisel
                       : Animation::HumanoidConstructionPoseKind::Chisel,
      .work_phase = work_phase,
      .jitter_seed = m_anim_ctx.jitter_seed,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_construction_body_deltas(m_pose, sample);
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
}

void HumanoidPoseController::spear_thrust(float attack_phase) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::SpearThrustClassic,
                                m_anim_ctx,
                                attack_phase,
                                0U,
                                1.0F));
  apply_spear_attack_sample(*this, m_pose, sample);
}

void HumanoidPoseController::spear_thrust_from_hold(float attack_phase,
                                                    float hold_depth) {
  auto const sample =
      Animation::resolve_humanoid_weapon_attack_pose(weapon_attack_pose_inputs(
          Animation::HumanoidWeaponAttackKind::SpearThrustFromHold,
          m_anim_ctx,
          attack_phase,
          0U,
          1.0F,
          hold_depth));
  apply_spear_attack_sample(*this, m_pose, sample);
  aim_held_weapon(m_pose,
                  spear_qvec_from_pose(sample.offhand_spear_direction),
                  baked_spear_direction());
}

void HumanoidPoseController::sword_slash(float attack_phase) {
  combat_sword_slash_variant(attack_phase, 0U);
}

void HumanoidPoseController::combat_sword_slash_variant(float attack_phase,
                                                        std::uint8_t variant,
                                                        float reach_scale) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::CombatSwordSlash,
                                m_anim_ctx,
                                attack_phase,
                                variant,
                                reach_scale));
  apply_weapon_attack_sample(*this, m_pose, sample);
  steer_melee_swing(attack_phase,
                    Animation::melee_intent_for_attack_variant(variant),
                    0.0F,
                    baked_sword_direction());
}

void HumanoidPoseController::mount_on_horse(float saddle_height) {
  m_pose.pelvis_pos.setY(saddle_height);
}

void HumanoidPoseController::hold_spear_idle() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SpearIdle,
      .shoulder_y = HP::SHOULDER_Y,
      .moving =
          Render::Creature::is_moving_animation(m_anim_ctx.inputs.movement_state) ||
          m_anim_ctx.gait.speed > 0.1F,
      .running = is_running_locomotion(m_anim_ctx),
      .sample_time = m_anim_ctx.inputs.time,
      .cycle_phase = m_anim_ctx.gait.cycle_phase,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::channel_spell_idle() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::CasterChannel,
      .shoulder_y = HP::SHOULDER_Y,
      .running = is_running_locomotion(m_anim_ctx),
      .sample_time = m_anim_ctx.inputs.time,
      .cycle_phase = m_anim_ctx.gait.cycle_phase,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::carry_stave() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::StaveCarry,
      .shoulder_y = HP::SHOULDER_Y,
      .running = is_running_locomotion(m_anim_ctx),
      .sample_time = m_anim_ctx.inputs.time,
      .cycle_phase = m_anim_ctx.gait.cycle_phase,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::wade_arms_raised(Animation::HumanoidHeldPoseKind kind) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = kind,
      .shoulder_y = HP::SHOULDER_Y,
      .sample_time = m_anim_ctx.inputs.time,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::carry_resource_load() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::ResourceCarry,
      .shoulder_y = HP::SHOULDER_Y,
      .sample_time = m_anim_ctx.inputs.time,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::brace_spear_for_hold() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SpearBrace,
      .shoulder_y = HP::SHOULDER_Y,
      .sample_time = m_anim_ctx.inputs.time,
  });
  apply_held_pose_sample(*this, m_pose, sample);
  aim_held_weapon(m_pose,
                  spear_qvec_from_pose(sample.offhand_spear_direction),
                  baked_spear_direction());
}

void HumanoidPoseController::hold_bow_ready() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::BowReady,
      .shoulder_y = HP::SHOULDER_Y,
      .sample_time = m_anim_ctx.inputs.time,
  });
  apply_held_pose_sample(*this, m_pose, sample);
  aim_held_weapon(m_pose, QVector3D(0.0F, 0.90F, 0.44F), k_baked_bow_axis);
}

void HumanoidPoseController::rest_bow_idle(float cycle_phase) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::BowRest,
      .shoulder_y = HP::SHOULDER_Y,
      .cycle_phase = cycle_phase,
  });
  apply_held_pose_sample(*this, m_pose, sample);
  aim_held_weapon(m_pose, QVector3D(-0.10F, 0.86F, 0.50F), k_baked_bow_axis);
}

void HumanoidPoseController::guard_sword_and_shield_for_defense() {
  guard_sword_and_shield_formation(ShieldFormationPose::GuardDefault, 1.0F);
}

void HumanoidPoseController::guard_sword_and_shield_formation(ShieldFormationPose pose,
                                                              float amount) {
  using HP = HumanProportions;
  auto const sample = Animation::resolve_humanoid_guard_stance_pose({
      .pose = pose,
      .amount = amount,
      .shoulder_y = HP::SHOULDER_Y,
  });
  if (!sample.active) {
    return;
  }

  auto const blend = [](const QVector3D& from, const QVector3D& to, float amount) {
    return from + (to - from) * amount;
  };
  place_hand_at(Side::Right,
                blend(m_pose.hand_r, to_qvec(sample.right_hand), sample.blend_amount));
  place_hand_at(Side::Left,
                blend(m_pose.hand_l, to_qvec(sample.left_hand), sample.blend_amount));

  m_pose.shoulder_l += to_qvec(sample.shoulder_l_delta);
  m_pose.shoulder_r += to_qvec(sample.shoulder_r_delta);
  m_pose.neck_base += to_qvec(sample.neck_delta);
  m_pose.head_pos += to_qvec(sample.head_delta);

  if (sample.blend_amount > 0.5F) {
    aim_held_weapon(
        m_pose, QVector3D(0.28F, 0.45F, 0.85F).normalized(), baked_sword_direction());
  }
}

void HumanoidPoseController::carry_sword_and_shield() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SwordShieldCarry,
      .shoulder_y = HP::SHOULDER_Y,
      .moving =
          Render::Creature::is_moving_animation(m_anim_ctx.inputs.movement_state) ||
          m_anim_ctx.gait.speed > 0.1F,
      .running = is_running_locomotion(m_anim_ctx),
      .cycle_phase = m_anim_ctx.gait.cycle_phase,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::look_at(const QVector3D& target) {
  auto const sample = Animation::resolve_humanoid_look_at_pose({
      .head_position = to_pose_vec(m_pose.head_pos),
      .target = to_pose_vec(target),
  });
  apply_posture_delta_sample(m_pose, sample);
}

void HumanoidPoseController::hit_flinch(float intensity) {
  auto const sample = Animation::resolve_humanoid_hit_flinch_pose({
      .intensity = intensity,
  });
  apply_posture_delta_sample(m_pose, sample);
}

void HumanoidPoseController::sword_slash_variant(float attack_phase,
                                                 std::uint8_t variant,
                                                 float reach_scale) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::SwordSlash,
                                m_anim_ctx,
                                attack_phase,
                                variant,
                                reach_scale));
  apply_weapon_attack_sample(*this, m_pose, sample);
  steer_melee_swing(attack_phase,
                    Animation::melee_intent_for_attack_variant(variant),
                    0.0F,
                    baked_sword_direction());
}

} // namespace Render::GL
