#include "pose_controller.h"

#include <QVector3D>

#include <algorithm>

#include "animation/ambient_pose_manifest.h"
#include "animation/attack_pose_manifest.h"
#include "animation/hold_pose_manifest.h"
#include "animation/posture_pose_manifest.h"
#include "animation/showcase_pose_manifest.h"
#include "grip_axis.h"
#include "humanoid_math.h"
#include "pose_primitives.h"
#include "render/creature/movement_state.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "spear_pose_utils.h"

namespace Render::GL {

namespace {

auto to_qvec(const Animation::PoseVec3& value) -> QVector3D {
  return {value.x, value.y, value.z};
}

const QVector3D k_baked_bow_axis(0.0F, 1.0F, 0.0F);

auto baked_spear_direction() -> QVector3D {
  return resolve_spear_direction(AnimationInputs{});
}

void aim_held_weapon(HumanoidPose& pose,
                     const QVector3D& wanted_direction,
                     const QVector3D& baked_direction) {
  pose.grip_axis_r = Render::Humanoid::hand_axis_for_weapon_direction(
      wanted_direction, baked_direction, true);
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
  auto clamp_to_attack_reach = [](const QVector3D& shoulder, const QVector3D& target) {
    return Render::Humanoid::PosePrimitives::clamp_to_reach(
        shoulder,
        target,
        Render::Humanoid::PosePrimitives::humanoid_arm_reach_limit(
            1.0F, Render::Humanoid::PosePrimitives::k_committed_arm_reach_fraction));
  };
  controller.place_hand_at(
      Side::Right, clamp_to_attack_reach(pose.shoulder_r, to_qvec(sample.right_hand)));
  controller.place_hand_at(
      Side::Left, clamp_to_attack_reach(pose.shoulder_l, to_qvec(sample.left_hand)));

  if (sample.has_blade_direction) {
    aim_held_weapon(pose, to_qvec(sample.blade_direction), baked_sword_direction());
  }
}

void apply_spear_attack_sample(
    HumanoidPoseController& controller,
    HumanoidPose& pose,
    const Animation::HumanoidWeaponAttackPoseSample& sample) {
  apply_weapon_attack_body_deltas(pose, sample);

  auto clamp_to_arm_reach = [](const QVector3D& shoulder, const QVector3D& target) {
    return Render::Humanoid::PosePrimitives::clamp_to_reach(
        shoulder,
        target,
        Render::Humanoid::PosePrimitives::humanoid_arm_reach_limit(
            1.0F, Render::Humanoid::PosePrimitives::k_braced_arm_reach_fraction));
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

  if (sample.use_offhand_spear_grip) {
    hand_l_target = Render::Humanoid::PosePrimitives::clamp_to_reach(
        pose.shoulder_l,
        hand_l_target,
        Render::Humanoid::PosePrimitives::humanoid_arm_reach_limit(
            1.0F, Render::Humanoid::PosePrimitives::k_braced_arm_reach_fraction));
  }

  apply_held_pose_body_deltas(pose, sample);
  controller.place_hand_at(Side::Right, hand_r_target);
  controller.place_hand_at(Side::Left, hand_l_target);
}

} // namespace

auto baked_sword_direction() -> QVector3D {

  auto const& grip = Render::Humanoid::humanoid_bind_body_frames().grip_r;
  QVector3D direction = (grip.right * k_sword_blade_axis_in_grip.x()) +
                        (grip.up * k_sword_blade_axis_in_grip.y()) +
                        (grip.forward * k_sword_blade_axis_in_grip.z());
  if (direction.lengthSquared() < 1.0e-8F) {
    return {0.0F, 1.0F, 0.0F};
  }
  direction.normalize();
  return direction;
}

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

void HumanoidPoseController::apply_idle_breath(float phase, bool mounted) {
  auto const sample = Animation::resolve_humanoid_idle_breath_pose({
      .phase = phase,
      .mounted = mounted,
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

void HumanoidPoseController::apply_showcase_move(Animation::HumanoidShowcaseMove move,
                                                 float phase) {
  auto const sample = Animation::resolve_humanoid_showcase_pose({
      .move = move,
      .phase = phase,
      .height_scale = 1.0F,
  });
  if (!sample.active) {
    return;
  }
  m_pose.pelvis_pos = to_qvec(sample.pelvis);
  m_pose.neck_base = to_qvec(sample.neck_base);
  m_pose.head_pos = to_qvec(sample.head);
  m_pose.shoulder_l = to_qvec(sample.shoulder_l);
  m_pose.shoulder_r = to_qvec(sample.shoulder_r);
  m_pose.elbow_l = to_qvec(sample.elbow_l);
  m_pose.elbow_r = to_qvec(sample.elbow_r);
  m_pose.hand_l = to_qvec(sample.hand_l);
  m_pose.hand_r = to_qvec(sample.hand_r);
  m_pose.knee_l = to_qvec(sample.knee_l);
  m_pose.knee_r = to_qvec(sample.knee_r);
  m_pose.foot_l = to_qvec(sample.foot_l);
  m_pose.foot_r = to_qvec(sample.foot_r);
  m_pose.foot_y_offset = 0.0F;
  if (sample.has_grip_axis) {
    m_pose.grip_axis_r = to_qvec(sample.grip_axis_r).normalized();
  }
}

void HumanoidPoseController::apply_death_collapse(
    Animation::HumanoidDeathCollapse collapse, float phase) {
  auto const sample = Animation::resolve_humanoid_death_pose({
      .collapse = collapse,
      .phase = phase,
      .height_scale = 1.0F,
  });
  if (!sample.active) {
    return;
  }
  m_pose.pelvis_pos = to_qvec(sample.pelvis);
  m_pose.neck_base = to_qvec(sample.neck_base);
  m_pose.head_pos = to_qvec(sample.head);
  m_pose.shoulder_l = to_qvec(sample.shoulder_l);
  m_pose.shoulder_r = to_qvec(sample.shoulder_r);
  m_pose.elbow_l = to_qvec(sample.elbow_l);
  m_pose.elbow_r = to_qvec(sample.elbow_r);
  m_pose.hand_l = to_qvec(sample.hand_l);
  m_pose.hand_r = to_qvec(sample.hand_r);
  m_pose.knee_l = to_qvec(sample.knee_l);
  m_pose.knee_r = to_qvec(sample.knee_r);
  m_pose.foot_l = to_qvec(sample.foot_l);
  m_pose.foot_r = to_qvec(sample.foot_r);
  m_pose.foot_pitch_l = sample.foot_pitch_l;
  m_pose.foot_pitch_r = sample.foot_pitch_r;
  m_pose.foot_y_offset = 0.0F;
}

void HumanoidPoseController::kneel(float depth) {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_kneel_pose({
      .depth = depth,
      .waist_y = HP::WAIST_Y,
      .ground_y = HP::GROUND_Y,
      .lower_leg_len = HP::LOWER_LEG_LEN,
      .foot_y_offset = m_pose.foot_y_offset,
      .knee_radius = HP::LOWER_LEG_R,
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
                                           const QVector3D& requested_position) {
  const QVector3D& shoulder = get_shoulder(side);

  QVector3D const target_position = Render::Humanoid::PosePrimitives::clamp_to_reach(
      shoulder,
      requested_position,
      Render::Humanoid::PosePrimitives::humanoid_arm_reach_limit());
  get_hand(side) = target_position;

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
  apply_bow_draw_body_deltas(m_pose, sample);
  place_hand_at(Side::Right, to_qvec(sample.right_hand));
  place_hand_at(Side::Left, to_qvec(sample.left_hand));
  aim_held_weapon(m_pose, QVector3D(0.0F, 1.0F, 0.0F), k_baked_bow_axis);
}

void HumanoidPoseController::bow_melee_strike(float attack_phase) {
  auto const sample = Animation::resolve_humanoid_weapon_attack_pose(
      weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind::BowMeleeStrike,
                                m_anim_ctx,
                                attack_phase,
                                0U,
                                1.0F));
  apply_weapon_attack_sample(*this, m_pose, sample);
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
}

void HumanoidPoseController::mount_on_horse(float saddle_height) {
  m_pose.pelvis_pos.setY(saddle_height);
}

void HumanoidPoseController::hold_spear_idle() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SpearIdle,
      .shoulder_y = HP::SHOULDER_Y,
      .sample_time = m_anim_ctx.inputs.time,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::channel_spell_idle() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::CasterChannel,
      .shoulder_y = HP::SHOULDER_Y,
      .sample_time = m_anim_ctx.inputs.time,
  });
  apply_held_pose_sample(*this, m_pose, sample);
}

void HumanoidPoseController::carry_stave() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::StaveCarry,
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
}

void HumanoidPoseController::carry_sword_and_shield() {
  using HP = HumanProportions;

  auto const sample = Animation::resolve_humanoid_held_pose({
      .kind = Animation::HumanoidHeldPoseKind::SwordShieldCarry,
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
  aim_held_weapon(m_pose,
                  spear_qvec_from_pose(sample.offhand_spear_direction),
                  baked_spear_direction());
}

void HumanoidPoseController::crouch(float depth) {
  using HP = HumanProportions;
  if (depth <= 0.0005F) {
    return;
  }
  m_pose.pelvis_pos.setY(m_pose.pelvis_pos.y() - depth);
  Animation::HumanoidPostureDeltaSample drop{};
  drop.shoulder_l_y_delta = -depth;
  drop.shoulder_r_y_delta = -depth;
  drop.neck_y_delta = -depth;
  drop.head_y_delta = -depth;
  drop.hand_l_y_delta = -depth;
  drop.hand_r_y_delta = -depth;
  drop.torso_frame_origin_delta = {0.0F, -depth, 0.0F};
  drop.head_frame_origin_delta = {0.0F, -depth, 0.0F};
  apply_posture_delta_sample(m_pose, drop);
  m_pose.elbow_l.setY(m_pose.elbow_l.y() - depth);
  m_pose.elbow_r.setY(m_pose.elbow_r.y() - depth);

  QVector3D const hip_l =
      m_pose.pelvis_pos +
      QVector3D(-HP::HIP_LATERAL_OFFSET, HP::HIP_VERTICAL_OFFSET, 0.0F);
  QVector3D const hip_r =
      m_pose.pelvis_pos +
      QVector3D(HP::HIP_LATERAL_OFFSET, HP::HIP_VERTICAL_OFFSET, 0.0F);
  m_pose.knee_l = solve_knee_ik(Side::Left, hip_l, m_pose.foot_l, 1.0F);
  m_pose.knee_r = solve_knee_ik(Side::Right, hip_r, m_pose.foot_r, 1.0F);
}

namespace {

void apply_ready_weapon_carriage(HumanoidPoseController& controller,
                                 Animation::HumanoidReadyWeapon weapon,
                                 float weapon_phase,
                                 float reach_scale) {
  switch (weapon) {
  case Animation::HumanoidReadyWeapon::SwordAndShield:
    controller.combat_sword_slash_variant(weapon_phase, 0U, reach_scale);
    break;
  case Animation::HumanoidReadyWeapon::Spear:
    controller.spear_thrust_variant(weapon_phase, 0U);
    break;
  case Animation::HumanoidReadyWeapon::None:
    break;
  }
}

} // namespace

void HumanoidPoseController::raise_shield_guard(float amount) {
  using HP = HumanProportions;
  auto const sample = Animation::resolve_humanoid_guard_stance_pose({
      .pose = ShieldFormationPose::GuardDefault,
      .amount = amount,
      .shoulder_y = HP::SHOULDER_Y,
  });
  if (!sample.active) {
    return;
  }
  QVector3D const target = to_qvec(sample.left_hand);
  place_hand_at(Side::Left,
                m_pose.hand_l + (target - m_pose.hand_l) * sample.blend_amount);
  m_pose.shoulder_l += to_qvec(sample.shoulder_l_delta);
  m_pose.shoulder_r += to_qvec(sample.shoulder_r_delta) * 0.5F;
  m_pose.neck_base += to_qvec(sample.neck_delta) * 0.6F;
  m_pose.head_pos += to_qvec(sample.head_delta) * 0.6F;
}

void HumanoidPoseController::combat_ready_stance(float phase,
                                                 Animation::HumanoidReadyWeapon weapon,
                                                 float reach_scale) {
  auto const sample = Animation::resolve_humanoid_ready_stance({
      .phase = phase,
      .weapon = weapon,
  });
  apply_ready_weapon_carriage(*this, weapon, sample.weapon_phase, reach_scale);
  if (weapon == Animation::HumanoidReadyWeapon::SwordAndShield &&
      sample.shield_raise > 0.0F) {
    raise_shield_guard(sample.shield_raise);
  }
  tilt_torso(sample.torso_side, sample.torso_forward);
  m_pose.pelvis_pos.setX(m_pose.pelvis_pos.x() + sample.sway_x);
  crouch(sample.crouch);
}

void HumanoidPoseController::melee_reaction(Animation::HumanoidReactionKind kind,
                                            float phase,
                                            Animation::HumanoidReadyWeapon weapon,
                                            float reach_scale) {
  auto const sample = Animation::resolve_humanoid_reaction_pose({
      .kind = kind,
      .phase = phase,
      .weapon = weapon,
  });
  apply_ready_weapon_carriage(*this, weapon, sample.weapon_phase, reach_scale);
  if (weapon == Animation::HumanoidReadyWeapon::SwordAndShield) {
    float const base_guard = 0.55F * (1.0F - sample.envelope);
    float const guard = std::clamp(base_guard + sample.shield_raise, 0.0F, 1.0F);
    if (guard > 0.0F) {
      raise_shield_guard(guard);
    }
  }
  if (sample.flinch > 0.0F) {
    hit_flinch(sample.flinch);
  }
  tilt_torso(sample.torso_side, sample.torso_forward);
  if (sample.head_back > 0.0F) {
    Animation::HumanoidPostureDeltaSample head{};
    head.head_z_delta = -sample.head_back;
    head.neck_z_delta = -sample.head_back * 0.5F;
    head.head_frame_origin_delta = {0.0F, 0.0F, -sample.head_back};
    apply_posture_delta_sample(m_pose, head);
  }
  QVector3D const right_axis = compute_right_axis();
  QVector3D const forward_axis = m_anim_ctx.heading_forward();
  auto const to_world = [&](const Animation::PoseVec3& delta) {
    return right_axis * delta.x + QVector3D(0.0F, delta.y, 0.0F) +
           forward_axis * delta.z;
  };
  QVector3D const hand_l = m_pose.hand_l + to_world(sample.hand_l_delta);
  QVector3D const hand_r = m_pose.hand_r + to_world(sample.hand_r_delta);
  place_hand_at(Side::Left, hand_l);
  place_hand_at(Side::Right, hand_r);
  crouch(sample.crouch);
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
