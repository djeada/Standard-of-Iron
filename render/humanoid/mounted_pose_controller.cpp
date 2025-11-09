#include "mounted_pose_controller.h"
#include "../horse/rig.h"
#include "humanoid_math.h"
#include "humanoid_specs.h"
#include "pose_controller.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL {

namespace {

auto seatRelative(const MountedAttachmentFrame &mount, float forward,
                  float right, float up) -> QVector3D {
  QVector3D const base = mount.seat_position + mount.ground_offset;
  return base + mount.seat_forward * forward + mount.seat_right * right +
         mount.seat_up * up;
}

auto reinAnchor(const MountedAttachmentFrame &mount, bool is_left, float slack,
                float tension) -> QVector3D {
  return compute_rein_handle(mount, is_left, slack, tension) +
         mount.ground_offset;
}

} // namespace

MountedPoseController::MountedPoseController(
    HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx)
    : m_pose(pose), m_anim_ctx(anim_ctx) {}

void MountedPoseController::mountOnHorse(const MountedAttachmentFrame &mount) {
  positionPelvisOnSaddle(mount);
  attachFeetToStirrups(mount);
  calculateRidingKnees(mount);
}

void MountedPoseController::dismount() {

  using HP = HumanProportions;
  m_pose.pelvisPos = QVector3D(0.0F, HP::WAIST_Y, 0.0F);
  m_pose.footL = QVector3D(-0.14F, HP::GROUND_Y + m_pose.footYOffset, 0.06F);
  m_pose.foot_r = QVector3D(0.14F, HP::GROUND_Y + m_pose.footYOffset, -0.06F);
}

void MountedPoseController::ridingIdle(const MountedAttachmentFrame &mount) {
  mountOnHorse(mount);

  QVector3D const left_hand_rest = seatRelative(mount, 0.12F, -0.14F, -0.05F);
  QVector3D const right_hand_rest = seatRelative(mount, 0.12F, 0.14F, -0.05F);

  getHand(true) = left_hand_rest;
  getHand(false) = right_hand_rest;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), left_hand_rest,
                                left_outward, 0.45F, 0.12F, -0.05F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), right_hand_rest,
                                 right_outward, 0.45F, 0.12F, -0.05F, 1.0F);
}

void MountedPoseController::ridingLeaning(const MountedAttachmentFrame &mount,
                                          float forward_lean, float side_lean) {
  forward_lean = std::clamp(forward_lean, -1.0F, 1.0F);
  side_lean = std::clamp(side_lean, -1.0F, 1.0F);

  mountOnHorse(mount);

  QVector3D const lean_offset = mount.seat_forward * (forward_lean * 0.15F) +
                                mount.seat_right * (side_lean * 0.10F);

  m_pose.shoulderL += lean_offset;
  m_pose.shoulderR += lean_offset;
  m_pose.neck_base += lean_offset * 0.9F;
  m_pose.headPos += lean_offset * 0.85F;

  float const relaxed_slack = 0.35F;
  float const relaxed_tension = 0.15F;
  QVector3D const left_hand =
      reinAnchor(mount, true, relaxed_slack, relaxed_tension) +
      lean_offset * 0.5F;
  QVector3D const right_hand =
      reinAnchor(mount, false, relaxed_slack, relaxed_tension) +
      lean_offset * 0.5F;

  getHand(true) = left_hand;
  getHand(false) = right_hand;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), left_hand,
                                left_outward, 0.48F, 0.10F, -0.08F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), right_hand,
                                 right_outward, 0.48F, 0.10F, -0.08F, 1.0F);
}

void MountedPoseController::ridingCharging(const MountedAttachmentFrame &mount,
                                           float intensity) {
  intensity = std::clamp(intensity, 0.0F, 1.0F);

  mountOnHorse(mount);

  QVector3D const charge_lean = mount.seat_forward * (0.25F * intensity);
  m_pose.shoulderL += charge_lean;
  m_pose.shoulderR += charge_lean;
  m_pose.neck_base += charge_lean * 0.85F;
  m_pose.headPos += charge_lean * 0.75F;

  float const crouch = 0.08F * intensity;
  m_pose.shoulderL.setY(m_pose.shoulderL.y() - crouch);
  m_pose.shoulderR.setY(m_pose.shoulderR.y() - crouch);
  m_pose.neck_base.setY(m_pose.neck_base.y() - crouch * 0.8F);
  m_pose.headPos.setY(m_pose.headPos.y() - crouch * 0.7F);

  holdReins(mount, 0.2F, 0.2F, 0.85F, 0.85F);
}

void MountedPoseController::ridingReining(const MountedAttachmentFrame &mount,
                                          float left_tension,
                                          float right_tension) {
  left_tension = std::clamp(left_tension, 0.0F, 1.0F);
  right_tension = std::clamp(right_tension, 0.0F, 1.0F);

  mountOnHorse(mount);

  QVector3D left_rein_pos = reinAnchor(mount, true, 0.15F, left_tension);
  QVector3D right_rein_pos = reinAnchor(mount, false, 0.15F, right_tension);

  getHand(true) = left_rein_pos;
  getHand(false) = right_rein_pos;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), left_rein_pos,
                                left_outward, 0.52F, 0.08F, -0.12F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), right_rein_pos,
                                 right_outward, 0.52F, 0.08F, -0.12F, 1.0F);

  float const avg_tension = (left_tension + right_tension) * 0.5F;
  QVector3D const lean_back = mount.seat_forward * (-0.08F * avg_tension);
  m_pose.shoulderL += lean_back;
  m_pose.shoulderR += lean_back;
  m_pose.neck_base += lean_back * 0.9F;
  m_pose.headPos += lean_back * 0.85F;
}

void MountedPoseController::ridingMeleeStrike(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mountOnHorse(mount);

  QVector3D const rest_pos = seatRelative(mount, 0.0F, 0.15F, 0.05F);
  QVector3D const raised_pos = seatRelative(mount, 0.0F, 0.20F, 0.50F);
  QVector3D const strike_pos = seatRelative(mount, 0.40F, 0.25F, -0.15F);

  QVector3D hand_r_target;
  QVector3D hand_l_target =
      reinAnchor(mount, true, 0.20F, 0.25F) + mount.seat_up * -0.02F;

  if (attack_phase < 0.30F) {

    float t = attack_phase / 0.30F;
    t = t * t;
    hand_r_target = rest_pos * (1.0F - t) + raised_pos * t;
  } else if (attack_phase < 0.50F) {

    float t = (attack_phase - 0.30F) / 0.20F;
    t = t * t * t;
    hand_r_target = raised_pos * (1.0F - t) + strike_pos * t;
    hand_r_target += mount.seat_up * (-0.25F - 0.12F * t);

    QVector3D lean = mount.seat_forward * (0.12F * t);
    m_pose.shoulderL += lean;
    m_pose.shoulderR += lean;
    m_pose.neck_base += lean * 0.9F;
    m_pose.headPos += lean * 0.85F;
  } else {

    float t = (attack_phase - 0.50F) / 0.50F;
    t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_r_target = strike_pos * (1.0F - t) + rest_pos * t;
  }

  getHand(false) = hand_r_target;
  getHand(true) = hand_l_target;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), hand_l_target,
                                left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), hand_r_target,
                                 right_outward, 0.42F, 0.15F, 0.0F, 1.0F);
}

void MountedPoseController::ridingSpearThrust(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mountOnHorse(mount);

  QVector3D const guard_pos = seatRelative(mount, 0.10F, 0.15F, 0.10F);
  QVector3D const thrust_pos = seatRelative(mount, 0.85F, 0.10F, 0.15F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  if (attack_phase < 0.25F) {

    float t = attack_phase / 0.25F;
    hand_r_target = guard_pos;
    hand_l_target = guard_pos - mount.seat_right * 0.25F;
  } else if (attack_phase < 0.45F) {

    float t = (attack_phase - 0.25F) / 0.20F;
    t = t * t * t;
    hand_r_target = guard_pos * (1.0F - t) + thrust_pos * t;
    hand_l_target = (guard_pos - mount.seat_right * 0.25F) * (1.0F - t) +
                    (thrust_pos - mount.seat_right * 0.30F) * t;

    QVector3D lean = mount.seat_forward * (0.18F * t);
    m_pose.shoulderL += lean;
    m_pose.shoulderR += lean;
    m_pose.neck_base += lean * 0.9F;
    m_pose.headPos += lean * 0.85F;
  } else {

    float t = (attack_phase - 0.45F) / 0.55F;
    t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_r_target = thrust_pos * (1.0F - t) + guard_pos * t;
    hand_l_target = (thrust_pos - mount.seat_right * 0.30F) * (1.0F - t) +
                    (guard_pos - mount.seat_right * 0.25F) * t;
  }

  getHand(false) = hand_r_target;
  getHand(true) = hand_l_target;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), hand_l_target,
                                left_outward, 0.48F, 0.10F, -0.06F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), hand_r_target,
                                 right_outward, 0.48F, 0.10F, -0.04F, 1.0F);
}

void MountedPoseController::ridingBowShot(const MountedAttachmentFrame &mount,
                                          float draw_phase) {
  draw_phase = std::clamp(draw_phase, 0.0F, 1.0F);

  mountOnHorse(mount);

  QVector3D const bow_hold_pos = seatRelative(mount, 0.25F, -0.08F, 0.25F);
  QVector3D const draw_start_pos =
      bow_hold_pos + mount.seat_right * 0.08F + QVector3D(0.0F, -0.05F, 0.0F);
  QVector3D const draw_end_pos = seatRelative(mount, 0.0F, 0.12F, 0.18F);

  QVector3D hand_l_target = bow_hold_pos;
  QVector3D hand_r_target;

  if (draw_phase < 0.30F) {

    float t = draw_phase / 0.30F;
    t = t * t;
    hand_r_target = draw_start_pos * (1.0F - t) + draw_end_pos * t;
  } else if (draw_phase < 0.65F) {

    hand_r_target = draw_end_pos;
  } else {

    float t = (draw_phase - 0.65F) / 0.35F;
    t = t * t * t;
    hand_r_target = draw_end_pos * (1.0F - t) + draw_start_pos * t;
  }

  getHand(true) = hand_l_target;
  getHand(false) = hand_r_target;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), hand_l_target,
                                left_outward, 0.50F, 0.08F, -0.05F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), hand_r_target,
                                 right_outward, 0.48F, 0.12F, -0.08F, 1.0F);
}

void MountedPoseController::ridingShieldDefense(
    const MountedAttachmentFrame &mount, bool raised) {
  mountOnHorse(mount);

  QVector3D shield_pos;
  if (raised) {

    shield_pos = seatRelative(mount, 0.15F, -0.18F, 0.40F);
  } else {

    shield_pos = seatRelative(mount, 0.0F, -0.15F, 0.08F);
  }

  float const rein_slack = raised ? 0.15F : 0.30F;
  float const rein_tension = raised ? 0.45F : 0.25F;
  QVector3D const rein_pos = reinAnchor(mount, false, rein_slack, rein_tension);

  getHand(true) = shield_pos;
  getHand(false) = rein_pos;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), shield_pos,
                                left_outward, 0.45F, 0.15F, -0.10F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), rein_pos,
                                 right_outward, 0.45F, 0.12F, -0.08F, 1.0F);
}

void MountedPoseController::holdReins(const MountedAttachmentFrame &mount,
                                      float left_slack, float right_slack,
                                      float left_tension, float right_tension) {
  left_slack = std::clamp(left_slack, 0.0F, 1.0F);
  right_slack = std::clamp(right_slack, 0.0F, 1.0F);
  left_tension = std::clamp(left_tension, 0.0F, 1.0F);
  right_tension = std::clamp(right_tension, 0.0F, 1.0F);

  QVector3D const left_rein_pos =
      reinAnchor(mount, true, left_slack, left_tension);
  QVector3D const right_rein_pos =
      reinAnchor(mount, false, right_slack, right_tension);

  getHand(true) = left_rein_pos;
  getHand(false) = right_rein_pos;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), left_rein_pos,
                                left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), right_rein_pos,
                                 right_outward, 0.45F, 0.12F, -0.08F, 1.0F);
}

void MountedPoseController::holdSpearMounted(
    const MountedAttachmentFrame &mount, SpearGrip grip_style) {
  mountOnHorse(mount);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  switch (grip_style) {
  case SpearGrip::OVERHAND:

    hand_r_target = seatRelative(mount, 0.0F, 0.12F, 0.55F);
    hand_l_target = reinAnchor(mount, true, 0.30F, 0.30F);
    break;

  case SpearGrip::COUCHED:

    hand_r_target = seatRelative(mount, -0.15F, 0.08F, 0.08F);
    hand_l_target = reinAnchor(mount, true, 0.35F, 0.20F);
    break;

  case SpearGrip::TWO_HANDED:

    hand_r_target = seatRelative(mount, 0.15F, 0.15F, 0.12F);
    hand_l_target = hand_r_target - mount.seat_right * 0.25F;
    break;
  }

  getHand(false) = hand_r_target;
  getHand(true) = hand_l_target;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), hand_l_target,
                                left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), hand_r_target,
                                 right_outward, 0.45F, 0.12F, -0.05F, 1.0F);
}

void MountedPoseController::holdBowMounted(
    const MountedAttachmentFrame &mount) {
  mountOnHorse(mount);

  QVector3D const bow_hold_pos = seatRelative(mount, 0.20F, -0.08F, 0.22F);
  QVector3D const arrow_nock_pos =
      bow_hold_pos + mount.seat_right * 0.10F + mount.seat_up * -0.02F;

  getHand(true) = bow_hold_pos;
  getHand(false) = arrow_nock_pos;

  const QVector3D left_outward = computeOutwardDir(true);
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(true) = solveElbowIK(true, getShoulder(true), bow_hold_pos,
                                left_outward, 0.48F, 0.10F, -0.05F, 1.0F);
  getElbow(false) = solveElbowIK(false, getShoulder(false), arrow_nock_pos,
                                 right_outward, 0.48F, 0.10F, -0.06F, 1.0F);
}

void MountedPoseController::attachFeetToStirrups(
    const MountedAttachmentFrame &mount) {

  m_pose.footL = mount.stirrup_bottom_left + mount.ground_offset;
  m_pose.foot_r = mount.stirrup_bottom_right + mount.ground_offset;
}

void MountedPoseController::positionPelvisOnSaddle(
    const MountedAttachmentFrame &mount) {
  QVector3D const seat_world = mount.seat_position + mount.ground_offset;
  QVector3D const delta = seat_world - m_pose.pelvisPos;
  m_pose.pelvisPos = seat_world;
  translateUpperBody(delta);
}

void MountedPoseController::translateUpperBody(const QVector3D &delta) {
  m_pose.shoulderL += delta;
  m_pose.shoulderR += delta;
  m_pose.neck_base += delta;
  m_pose.headPos += delta;
  m_pose.elbowL += delta;
  m_pose.elbowR += delta;
  m_pose.handL += delta;
  m_pose.hand_r += delta;
}

void MountedPoseController::calculateRidingKnees(
    const MountedAttachmentFrame &mount) {
  using MRP = MountedRiderProportions;

  QVector3D const hip_offset = mount.seat_up * -0.02F;
  QVector3D const hip_left =
      m_pose.pelvisPos - mount.seat_right * 0.10F + hip_offset;
  QVector3D const hip_right =
      m_pose.pelvisPos + mount.seat_right * 0.10F + hip_offset;

  float const height_scale = m_anim_ctx.variation.height_scale;

  // Use shortened limb lengths for mounted riders
  LimbLengths mounted_limbs;
  mounted_limbs.upper_leg = MRP::UPPER_LEG_LEN;
  mounted_limbs.lower_leg = MRP::LOWER_LEG_LEN;
  mounted_limbs.upper_arm = MRP::UPPER_ARM_LEN;
  mounted_limbs.fore_arm = MRP::FORE_ARM_LEN;

  m_pose.knee_l = solveKneeIK(true, hip_left, m_pose.footL, height_scale, mounted_limbs);
  m_pose.knee_r = solveKneeIK(false, hip_right, m_pose.foot_r, height_scale, mounted_limbs);
}

auto MountedPoseController::solveElbowIK(
    bool, const QVector3D &shoulder, const QVector3D &hand,
    const QVector3D &outward_dir, float along_frac, float lateral_offset,
    float y_bias, float outward_sign) const -> QVector3D {
  return elbowBendTorso(shoulder, hand, outward_dir, along_frac, lateral_offset,
                        y_bias, outward_sign);
}

auto MountedPoseController::solveKneeIK(bool is_left, const QVector3D &hip,
                                        const QVector3D &foot,
                                        float height_scale, const LimbLengths &limbs) const -> QVector3D {
  using HP = HumanProportions;

  QVector3D hip_to_foot = foot - hip;
  float const distance = hip_to_foot.length();
  if (distance < 1e-5F) {
    return hip;
  }

  // Use custom limb lengths for mounted riders
  float const upper_len = limbs.upper_leg * height_scale;
  float const lower_len = limbs.lower_leg * height_scale;
  float const reach = upper_len + lower_len;
  float const min_reach =
      std::max(std::abs(upper_len - lower_len) + 1e-4F, 1e-3F);
  float const max_reach = std::max(reach - 1e-4F, min_reach + 1e-4F);
  float const clamped_dist = std::clamp(distance, min_reach, max_reach);

  QVector3D const dir = hip_to_foot / distance;

  float cos_theta = (upper_len * upper_len + clamped_dist * clamped_dist -
                     lower_len * lower_len) /
                    (2.0F * upper_len * clamped_dist);
  cos_theta = std::clamp(cos_theta, -1.0F, 1.0F);
  float const sin_theta =
      std::sqrt(std::max(0.0F, 1.0F - cos_theta * cos_theta));

  QVector3D bend_pref = is_left ? QVector3D(-0.70F, -0.15F, 0.30F)
                                : QVector3D(0.70F, -0.15F, 0.30F);
  bend_pref.normalize();

  QVector3D bend_axis = bend_pref - dir * QVector3D::dotProduct(dir, bend_pref);
  if (bend_axis.lengthSquared() < 1e-6F) {
    bend_axis = QVector3D::crossProduct(dir, QVector3D(0.0F, 1.0F, 0.0F));
    if (bend_axis.lengthSquared() < 1e-6F) {
      bend_axis = QVector3D::crossProduct(dir, QVector3D(1.0F, 0.0F, 0.0F));
    }
  }
  bend_axis.normalize();

  QVector3D knee =
      hip + dir * (cos_theta * upper_len) + bend_axis * (sin_theta * upper_len);

  float const knee_floor = HP::GROUND_Y + m_pose.footYOffset * 0.5F;
  if (knee.y() < knee_floor) {
    knee.setY(knee_floor);
  }

  if (knee.y() > hip.y()) {
    knee.setY(hip.y());
  }

  return knee;
}

auto MountedPoseController::getShoulder(bool is_left) const
    -> const QVector3D & {
  return is_left ? m_pose.shoulderL : m_pose.shoulderR;
}

auto MountedPoseController::getHand(bool is_left) -> QVector3D & {
  return is_left ? m_pose.handL : m_pose.hand_r;
}

auto MountedPoseController::getHand(bool is_left) const -> const QVector3D & {
  return is_left ? m_pose.handL : m_pose.hand_r;
}

auto MountedPoseController::getElbow(bool is_left) -> QVector3D & {
  return is_left ? m_pose.elbowL : m_pose.elbowR;
}

auto MountedPoseController::computeRightAxis() const -> QVector3D {
  QVector3D right_axis = m_pose.shoulderR - m_pose.shoulderL;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right_axis.normalize();
  return right_axis;
}

auto MountedPoseController::computeOutwardDir(bool is_left) const -> QVector3D {
  QVector3D const right_axis = computeRightAxis();
  return is_left ? -right_axis : right_axis;
}

} // namespace Render::GL
