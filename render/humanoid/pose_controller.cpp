#include "pose_controller.h"

#include <QVector3D>

#include <algorithm>

#include "../creature/movement_state.h"
#include "animation/ambient_pose_manifest.h"
#include "animation/attack_pose_manifest.h"
#include "animation/hold_pose_manifest.h"
#include "animation/posture_pose_manifest.h"
#include "humanoid_math.h"
#include "pose_primitives.h"
#include "spear_pose_utils.h"

namespace Render::GL {

namespace {

auto to_qvec(const Animation::PoseVec3& value) -> QVector3D {
  return {value.x, value.y, value.z};
}

auto to_pose_vec(const QVector3D& value) -> Animation::PoseVec3 {
  return {value.x(), value.y(), value.z()};
}

void apply_weapon_attack_body_deltas(
    HumanoidPose& pose, const Animation::HumanoidWeaponAttackPoseSample& sample) {
  pose.shoulder_l.setX(pose.shoulder_l.x() + sample.shoulder_l_x_delta);
  pose.shoulder_r.setX(pose.shoulder_r.x() + sample.shoulder_r_x_delta);
  pose.shoulder_l.setY(pose.shoulder_l.y() + sample.shoulder_l_y_delta);
  pose.shoulder_r.setY(pose.shoulder_r.y() + sample.shoulder_r_y_delta);
  pose.shoulder_l.setZ(pose.shoulder_l.z() + sample.shoulder_l_z_delta);
  pose.shoulder_r.setZ(pose.shoulder_r.z() + sample.shoulder_r_z_delta);
  pose.neck_base.setY(pose.neck_base.y() + sample.neck_y_delta);
  pose.neck_base.setZ(pose.neck_base.z() + sample.neck_z_delta);
  pose.head_pos.setY(pose.head_pos.y() + sample.head_y_delta);
  pose.head_pos.setZ(pose.head_pos.z() + sample.head_z_delta);
  pose.pelvis_pos.setY(pose.pelvis_pos.y() + sample.pelvis_y_delta);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() + sample.pelvis_z_delta);
  pose.foot_l.setZ(pose.foot_l.z() + sample.foot_l_z_delta);
  pose.knee_l.setZ(pose.knee_l.z() + sample.knee_l_z_delta);
  pose.foot_r.setZ(pose.foot_r.z() + sample.foot_r_z_delta);
  pose.knee_r.setZ(pose.knee_r.z() + sample.knee_r_z_delta);
}

void apply_bow_draw_body_deltas(HumanoidPose& pose,
                                const Animation::HumanoidBowDrawPoseSample& sample) {
  pose.shoulder_l.setY(pose.shoulder_l.y() + sample.shoulder_l_y_delta);
  pose.shoulder_r.setY(pose.shoulder_r.y() + sample.shoulder_r_y_delta);
  pose.shoulder_l.setZ(pose.shoulder_l.z() + sample.shoulder_l_z_delta);
  pose.shoulder_r.setZ(pose.shoulder_r.z() + sample.shoulder_r_z_delta);
  pose.neck_base.setY(pose.neck_base.y() + sample.neck_y_delta);
  pose.neck_base.setZ(pose.neck_base.z() + sample.neck_z_delta);
  pose.head_pos.setZ(pose.head_pos.z() + sample.head_z_delta);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() + sample.pelvis_z_delta);
}

void apply_construction_body_deltas(
    HumanoidPose& pose, const Animation::HumanoidConstructionPoseSample& sample) {
  pose.shoulder_l.setX(pose.shoulder_l.x() + sample.shoulder_l_x_delta);
  pose.shoulder_r.setX(pose.shoulder_r.x() + sample.shoulder_r_x_delta);
  pose.shoulder_l.setY(pose.shoulder_l.y() + sample.shoulder_l_y_delta);
  pose.shoulder_r.setY(pose.shoulder_r.y() + sample.shoulder_r_y_delta);
  pose.shoulder_l.setZ(pose.shoulder_l.z() + sample.shoulder_l_z_delta);
  pose.shoulder_r.setZ(pose.shoulder_r.z() + sample.shoulder_r_z_delta);
  pose.neck_base.setZ(pose.neck_base.z() + sample.neck_z_delta);
  pose.head_pos.setY(pose.head_pos.y() + sample.head_y_delta);
  pose.head_pos.setZ(pose.head_pos.z() + sample.head_z_delta);
  pose.pelvis_pos.setX(pose.pelvis_pos.x() + sample.pelvis_x_delta);
  pose.foot_l.setZ(pose.foot_l.z() + sample.foot_l_z_delta);
  pose.foot_r.setZ(pose.foot_r.z() + sample.foot_r_z_delta);
  pose.knee_l.setZ(pose.knee_l.z() + sample.knee_l_z_delta);
  pose.knee_r.setZ(pose.knee_r.z() + sample.knee_r_z_delta);
}

void apply_held_pose_body_deltas(HumanoidPose& pose,
                                 const Animation::HumanoidHeldPoseSample& sample) {
  pose.shoulder_l.setX(pose.shoulder_l.x() + sample.shoulder_l_x_delta);
  pose.shoulder_r.setX(pose.shoulder_r.x() + sample.shoulder_r_x_delta);
  pose.shoulder_l.setY(pose.shoulder_l.y() + sample.shoulder_l_y_delta);
  pose.shoulder_r.setY(pose.shoulder_r.y() + sample.shoulder_r_y_delta);
  pose.shoulder_l.setZ(pose.shoulder_l.z() + sample.shoulder_l_z_delta);
  pose.shoulder_r.setZ(pose.shoulder_r.z() + sample.shoulder_r_z_delta);
  pose.neck_base.setZ(pose.neck_base.z() + sample.neck_z_delta);
  pose.head_pos.setY(pose.head_pos.y() + sample.head_y_delta);
  pose.head_pos.setZ(pose.head_pos.z() + sample.head_z_delta);
}

void apply_ambient_pose_sample(HumanoidPose& pose,
                               const Animation::HumanoidAmbientPoseSample& sample) {
  pose.pelvis_pos.setX(pose.pelvis_pos.x() + sample.pelvis_x_delta);
  pose.pelvis_pos.setY(pose.pelvis_pos.y() + sample.pelvis_y_delta);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() + sample.pelvis_z_delta);
  pose.shoulder_l.setX(pose.shoulder_l.x() + sample.shoulder_l_x_delta);
  pose.shoulder_l.setY(pose.shoulder_l.y() + sample.shoulder_l_y_delta);
  pose.shoulder_l.setZ(pose.shoulder_l.z() + sample.shoulder_l_z_delta);
  pose.shoulder_r.setX(pose.shoulder_r.x() + sample.shoulder_r_x_delta);
  pose.shoulder_r.setY(pose.shoulder_r.y() + sample.shoulder_r_y_delta);
  pose.shoulder_r.setZ(pose.shoulder_r.z() + sample.shoulder_r_z_delta);
  pose.neck_base.setX(pose.neck_base.x() + sample.neck_x_delta);
  pose.neck_base.setY(pose.neck_base.y() + sample.neck_y_delta);
  pose.neck_base.setZ(pose.neck_base.z() + sample.neck_z_delta);
  pose.head_pos.setX(pose.head_pos.x() + sample.head_x_delta);
  pose.head_pos.setY(pose.head_pos.y() + sample.head_y_delta);
  pose.head_pos.setZ(pose.head_pos.z() + sample.head_z_delta);
  pose.knee_l.setY(pose.knee_l.y() + sample.knee_l_y_delta);
  pose.knee_l.setZ(pose.knee_l.z() + sample.knee_l_z_delta);
  pose.knee_r.setY(pose.knee_r.y() + sample.knee_r_y_delta);
  pose.knee_r.setZ(pose.knee_r.z() + sample.knee_r_z_delta);
  pose.foot_l.setX(pose.foot_l.x() + sample.foot_l_x_delta);
  pose.foot_l.setY(pose.foot_l.y() + sample.foot_l_y_delta);
  pose.foot_l.setZ(pose.foot_l.z() + sample.foot_l_z_delta);
  pose.foot_r.setX(pose.foot_r.x() + sample.foot_r_x_delta);
  pose.foot_r.setY(pose.foot_r.y() + sample.foot_r_y_delta);
  pose.foot_r.setZ(pose.foot_r.z() + sample.foot_r_z_delta);
  pose.hand_l.setX(pose.hand_l.x() + sample.hand_l_x_delta);
  pose.hand_l.setY(pose.hand_l.y() + sample.hand_l_y_delta);
  pose.hand_l.setZ(pose.hand_l.z() + sample.hand_l_z_delta);
  pose.hand_r.setX(pose.hand_r.x() + sample.hand_r_x_delta);
  pose.hand_r.setY(pose.hand_r.y() + sample.hand_r_y_delta);
  pose.hand_r.setZ(pose.hand_r.z() + sample.hand_r_z_delta);
  pose.elbow_l.setX(pose.elbow_l.x() + sample.elbow_l_x_delta);
  pose.elbow_l.setY(pose.elbow_l.y() + sample.elbow_l_y_delta);
  pose.elbow_l.setZ(pose.elbow_l.z() + sample.elbow_l_z_delta);
  pose.elbow_r.setX(pose.elbow_r.x() + sample.elbow_r_x_delta);
  pose.elbow_r.setY(pose.elbow_r.y() + sample.elbow_r_y_delta);
  pose.elbow_r.setZ(pose.elbow_r.z() + sample.elbow_r_z_delta);
}

void apply_posture_delta_sample(HumanoidPose& pose,
                                const Animation::HumanoidPostureDeltaSample& sample) {
  pose.pelvis_pos.setX(pose.pelvis_pos.x() + sample.pelvis_x_delta);
  pose.pelvis_pos.setY(pose.pelvis_pos.y() + sample.pelvis_y_delta);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() + sample.pelvis_z_delta);
  pose.shoulder_l.setX(pose.shoulder_l.x() + sample.shoulder_l_x_delta);
  pose.shoulder_l.setY(pose.shoulder_l.y() + sample.shoulder_l_y_delta);
  pose.shoulder_l.setZ(pose.shoulder_l.z() + sample.shoulder_l_z_delta);
  pose.shoulder_r.setX(pose.shoulder_r.x() + sample.shoulder_r_x_delta);
  pose.shoulder_r.setY(pose.shoulder_r.y() + sample.shoulder_r_y_delta);
  pose.shoulder_r.setZ(pose.shoulder_r.z() + sample.shoulder_r_z_delta);
  pose.neck_base.setX(pose.neck_base.x() + sample.neck_x_delta);
  pose.neck_base.setY(pose.neck_base.y() + sample.neck_y_delta);
  pose.neck_base.setZ(pose.neck_base.z() + sample.neck_z_delta);
  pose.head_pos.setX(pose.head_pos.x() + sample.head_x_delta);
  pose.head_pos.setY(pose.head_pos.y() + sample.head_y_delta);
  pose.head_pos.setZ(pose.head_pos.z() + sample.head_z_delta);
  pose.hand_l.setY(pose.hand_l.y() + sample.hand_l_y_delta);
  pose.hand_l.setZ(pose.hand_l.z() + sample.hand_l_z_delta);
  pose.hand_r.setY(pose.hand_r.y() + sample.hand_r_y_delta);
  pose.hand_r.setZ(pose.hand_r.z() + sample.hand_r_z_delta);
  pose.knee_l.setY(pose.knee_l.y() + sample.knee_l_y_delta);
  pose.knee_l.setZ(pose.knee_l.z() + sample.knee_l_z_delta);
  pose.knee_r.setZ(pose.knee_r.z() + sample.knee_r_z_delta);
  pose.foot_l.setZ(pose.foot_l.z() + sample.foot_l_z_delta);
  pose.foot_r.setZ(pose.foot_r.z() + sample.foot_r_z_delta);
  pose.body_frames.torso.origin += to_qvec(sample.torso_frame_origin_delta);
  pose.body_frames.head.origin += to_qvec(sample.head_frame_origin_delta);
}

auto weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind kind,
                               const HumanoidAnimationContext& anim_ctx,
                               float attack_phase,
                               std::uint8_t variant,
                               float reach_scale,
                               float hold_depth = 0.0F)
    -> Animation::HumanoidWeaponAttackPoseInputs {
  using HP = HumanProportions;
  return {
      .kind = kind,
      .attack_phase = attack_phase,
      .variant = variant,
      .reach_scale = reach_scale,
      .hold_depth = hold_depth,
      .attack_emphasis = anim_ctx.attack_emphasis,
      .finisher_attack = anim_ctx.finisher_attack,
      .shoulder_y = HP::SHOULDER_Y,
      .waist_y = HP::WAIST_Y,
  };
}

void apply_weapon_attack_sample(
    HumanoidPoseController& controller,
    HumanoidPose& pose,
    const Animation::HumanoidWeaponAttackPoseSample& sample) {
  apply_weapon_attack_body_deltas(pose, sample);
  auto clamp_to_attack_reach = [](const QVector3D& shoulder, QVector3D target) {
    constexpr float k_max_arm_reach =
        (HumanProportions::UPPER_ARM_LEN + HumanProportions::FORE_ARM_LEN) * 0.75F;
    QVector3D const shoulder_to_hand = target - shoulder;
    float const requested_reach = shoulder_to_hand.length();
    if (requested_reach > k_max_arm_reach && requested_reach > 1.0e-6F) {
      target = shoulder + shoulder_to_hand * (k_max_arm_reach / requested_reach);
    }
    return target;
  };
  controller.place_hand_at(
      Side::Right, clamp_to_attack_reach(pose.shoulder_r, to_qvec(sample.right_hand)));
  controller.place_hand_at(
      Side::Left, clamp_to_attack_reach(pose.shoulder_l, to_qvec(sample.left_hand)));
}

void apply_spear_attack_sample(
    HumanoidPoseController& controller,
    HumanoidPose& pose,
    const Animation::HumanoidWeaponAttackPoseSample& sample) {
  apply_weapon_attack_body_deltas(pose, sample);

  auto clamp_to_arm_reach = [](const QVector3D& shoulder, QVector3D target) {
    using HP = HumanProportions;
    constexpr float k_max_arm_reach = (HP::UPPER_ARM_LEN + HP::FORE_ARM_LEN) * 0.96F;
    QVector3D const shoulder_to_hand = target - shoulder;
    float const requested_reach = shoulder_to_hand.length();
    if (requested_reach > k_max_arm_reach) {
      target = shoulder + shoulder_to_hand * (k_max_arm_reach / requested_reach);
    }
    return target;
  };

  QVector3D const hand_r_target =
      clamp_to_arm_reach(pose.shoulder_r, to_qvec(sample.right_hand));
  QVector3D hand_l_target = to_qvec(sample.left_hand);
  if (sample.use_offhand_spear_grip) {
    hand_l_target = compute_offhand_spear_grip(pose,
                                               sample.offhand_spear_direction,
                                               hand_r_target,
                                               Side::Right,
                                               sample.offhand_along_offset,
                                               sample.offhand_y_drop,
                                               sample.offhand_lateral_offset);
  }
  hand_l_target = clamp_to_arm_reach(pose.shoulder_l, hand_l_target);
  controller.place_hand_at(Side::Right, hand_r_target);
  controller.place_hand_at(Side::Left, hand_l_target);
}

void apply_held_pose_sample(HumanoidPoseController& controller,
                            HumanoidPose& pose,
                            const Animation::HumanoidHeldPoseSample& sample) {
  QVector3D const hand_r_target = to_qvec(sample.right_hand);
  QVector3D hand_l_target = to_qvec(sample.left_hand);
  if (sample.use_offhand_spear_grip) {
    hand_l_target = compute_offhand_spear_grip(pose,
                                               sample.offhand_spear_direction,
                                               hand_r_target,
                                               Side::Right,
                                               sample.offhand_along_offset,
                                               sample.offhand_y_drop,
                                               sample.offhand_lateral_offset);
    if (sample.clamp_left_hand_x_min) {
      hand_l_target.setX(std::max(sample.left_hand_x_min, hand_l_target.x()));
    }
    if (sample.clamp_left_hand_y_max) {
      hand_l_target.setY(std::min(sample.left_hand_y_max, hand_l_target.y()));
    }
    hand_l_target.setZ(hand_l_target.z() + sample.left_hand_z_delta);
  }

  controller.place_hand_at(Side::Right, hand_r_target);
  controller.place_hand_at(Side::Left, hand_l_target);
  apply_held_pose_body_deltas(pose, sample);
}

} // namespace

HumanoidPoseController::HumanoidPoseController(HumanoidPose& pose,
                                               const HumanoidAnimationContext& anim_ctx)
    : m_pose(pose)
    , m_anim_ctx(anim_ctx) {
}

void HumanoidPoseController::stand_idle() {
}

void HumanoidPoseController::apply_micro_idle(float time, std::uint32_t seed) {
  auto const sample = Animation::resolve_humanoid_micro_idle_pose({
      .sample_time = time,
      .seed = seed,
  });
  apply_posture_delta_sample(m_pose, sample);
}

void HumanoidPoseController::apply_ambient_idle_explicit(AmbientIdleType idle_type,
                                                         float phase) {
  auto const sample = Animation::resolve_humanoid_ambient_pose({
      .type = idle_type,
      .phase = phase,
      .airborne = m_anim_ctx.gait.is_airborne,
  });
  apply_ambient_pose_sample(m_pose, sample);
}

void HumanoidPoseController::kneel(float depth) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_kneel_pose({
      .depth = depth,
      .waist_y = HP::WAIST_Y,
      .ground_y = HP::GROUND_Y,
      .lower_leg_len = HP::LOWER_LEG_LEN,
      .foot_y_offset = m_pose.foot_y_offset,
  });
  if (!sample.active) {
    return;
  }

  m_pose.pelvis_pos.setY(sample.pelvis.y);
  m_pose.knee_l = to_qvec(sample.knee_l);
  m_pose.foot_l = to_qvec(sample.foot_l);
  m_pose.knee_r = to_qvec(sample.knee_r);
  m_pose.foot_r = to_qvec(sample.foot_r);
  apply_posture_delta_sample(m_pose, sample.upper_body);
}

void HumanoidPoseController::kneel_transition(float progress, bool standing_up) {
  progress = std::clamp(progress, 0.0F, 1.0F);
  float const kneel_amount = standing_up ? (1.0F - progress) : progress;
  kneel(kneel_amount);

  auto const sample = Animation::resolve_humanoid_kneel_transition_pose({
      .progress = progress,
      .standing_up = standing_up,
  });
  apply_posture_delta_sample(m_pose, sample);
}

void HumanoidPoseController::lean(const QVector3D& direction, float amount) {
  auto const sample = Animation::resolve_humanoid_lean_pose({
      .direction = to_pose_vec(direction),
      .amount = amount,
  });
  apply_posture_delta_sample(m_pose, sample);
}

void HumanoidPoseController::place_hand_at(Side side,
                                           const QVector3D& target_position) {
  get_hand(side) = target_position;

  const QVector3D& shoulder = get_shoulder(side);
  const QVector3D outward_dir = compute_outward_dir(side);

  float const along_frac = (side == Side::Left) ? 0.45F : 0.48F;
  float const lateral_offset = (side == Side::Left) ? 0.15F : 0.12F;
  float const y_bias = (side == Side::Left) ? -0.08F : 0.02F;
  float const outward_sign = 1.0F;

  get_elbow(side) = solve_elbow_ik(side,
                                   shoulder,
                                   target_position,
                                   outward_dir,
                                   along_frac,
                                   lateral_offset,
                                   y_bias,
                                   outward_sign);
}

auto HumanoidPoseController::solve_elbow_ik(Side,
                                            const QVector3D& shoulder,
                                            const QVector3D& hand,
                                            const QVector3D& outward_dir,
                                            float along_frac,
                                            float lateral_offset,
                                            float y_bias,
                                            float outward_sign) const -> QVector3D {
  return Render::Humanoid::PosePrimitives::solve_elbow_ik(
      shoulder, hand, outward_dir, along_frac, lateral_offset, y_bias, outward_sign);
}

auto HumanoidPoseController::solve_knee_ik(Side side,
                                           const QVector3D& hip,
                                           const QVector3D& foot,
                                           float height_scale) const -> QVector3D {
  using HP = HumanProportions;
  return Render::Humanoid::PosePrimitives::solve_knee_ik(
      hip,
      foot,
      {.upper_leg_len = HP::UPPER_LEG_LEN * height_scale,
       .lower_leg_len = HP::LOWER_LEG_LEN * height_scale,
       .knee_floor = HP::GROUND_Y + m_pose.foot_y_offset * 0.5F,
       .bend_preference = (side == Side::Left) ? QVector3D(-0.24F, 0.0F, 0.95F)
                                               : QVector3D(0.24F, 0.0F, 0.95F),
       .clamp_to_hip_height = true});
}

auto HumanoidPoseController::get_shoulder(Side side) const -> const QVector3D& {
  return (side == Side::Left) ? m_pose.shoulder_l : m_pose.shoulder_r;
}

auto HumanoidPoseController::get_hand(Side side) -> QVector3D& {
  return (side == Side::Left) ? m_pose.hand_l : m_pose.hand_r;
}

auto HumanoidPoseController::get_hand(Side side) const -> const QVector3D& {
  return (side == Side::Left) ? m_pose.hand_l : m_pose.hand_r;
}

auto HumanoidPoseController::get_elbow(Side side) -> QVector3D& {
  return (side == Side::Left) ? m_pose.elbow_l : m_pose.elbow_r;
}

auto HumanoidPoseController::compute_right_axis() const -> QVector3D {
  return Render::Humanoid::PosePrimitives::compute_right_axis(m_pose);
}

auto HumanoidPoseController::compute_outward_dir(Side side) const -> QVector3D {
  return Render::Humanoid::PosePrimitives::compute_outward_dir(m_pose, side);
}

auto HumanoidPoseController::get_shoulder_y(Side side) const -> float {
  return (side == Side::Left) ? m_pose.shoulder_l.y() : m_pose.shoulder_r.y();
}

auto HumanoidPoseController::get_pelvis_y() const -> float {
  return m_pose.pelvis_pos.y();
}

void HumanoidPoseController::aim_bow(float draw_phase) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_bow_draw_pose({
      .draw_phase = draw_phase,
      .jitter_seed = m_anim_ctx.jitter_seed,
      .shoulder_y = HP::SHOULDER_Y,
  });
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  apply_bow_draw_body_deltas(m_pose, sample);
}

void HumanoidPoseController::melee_strike(float strike_phase) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::BasicMeleeStrike,
                                m_anim_ctx,
                                strike_phase,
                                0U,
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
  grasp_two_handed(to_qvec(sample.grip_center), sample.hand_separation);
  apply_construction_body_deltas(m_pose, sample);
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
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
  apply_construction_body_deltas(m_pose, sample);
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
}

void HumanoidPoseController::mount_on_horse(float saddle_height) {
  m_pose.pelvis_pos.setY(saddle_height);
}

void HumanoidPoseController::hold_spear_idle() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SpearIdle,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::brace_spear_for_hold() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SpearBrace,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::hold_bow_ready() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::BowReady,
      .shoulder_y = HP::SHOULDER_Y,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::brace_sword_and_shield_for_hold() {
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
}

void HumanoidPoseController::hold_sword_and_shield() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SwordShieldHold,
      .shoulder_y = HP::SHOULDER_Y,
      .moving =
          Render::Creature::is_moving_animation(m_anim_ctx.inputs.movement_state) ||
          m_anim_ctx.gait.speed > 0.1F,
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
}

void HumanoidPoseController::spear_thrust_variant(float attack_phase,
                                                  std::uint8_t variant) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::SpearThrust,
                                m_anim_ctx,
                                attack_phase,
                                variant,
                                1.0F));
  apply_spear_attack_sample(*this, m_pose, sample);
}

void HumanoidPoseController::tilt_torso(float side_tilt, float forward_tilt) {
  auto const sample = Animation::resolve_humanoid_torso_tilt_pose({
      .heading_right = to_pose_vec(m_anim_ctx.heading_right()),
      .heading_forward = to_pose_vec(m_anim_ctx.heading_forward()),
      .side_tilt = side_tilt,
      .forward_tilt = forward_tilt,
  });
  apply_posture_delta_sample(m_pose, sample);
}

} // namespace Render::GL
