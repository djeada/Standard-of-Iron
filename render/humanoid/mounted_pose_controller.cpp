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
  m_pose.pelvis_pos = QVector3D(0.0F, HP::WAIST_Y, 0.0F);
  m_pose.foot_l = QVector3D(-0.14F, HP::GROUND_Y + m_pose.foot_y_offset, 0.06F);
  m_pose.foot_r = QVector3D(0.14F, HP::GROUND_Y + m_pose.foot_y_offset, -0.06F);
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
  mountOnHorse(mount);
  applyLean(mount, forward_lean, side_lean);
}

void MountedPoseController::ridingCharging(const MountedAttachmentFrame &mount,
                                           float intensity) {
  intensity = std::clamp(intensity, 0.0F, 1.0F);

  mountOnHorse(mount);

  QVector3D const charge_lean = mount.seat_forward * (0.25F * intensity);
  m_pose.shoulder_l += charge_lean;
  m_pose.shoulder_r += charge_lean;
  m_pose.neck_base += charge_lean * 0.85F;
  m_pose.head_pos += charge_lean * 0.75F;

  float const crouch = 0.08F * intensity;
  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - crouch);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - crouch);
  m_pose.neck_base.setY(m_pose.neck_base.y() - crouch * 0.8F);
  m_pose.head_pos.setY(m_pose.head_pos.y() - crouch * 0.7F);

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
  m_pose.shoulder_l += lean_back;
  m_pose.shoulder_r += lean_back;
  m_pose.neck_base += lean_back * 0.9F;
  m_pose.head_pos += lean_back * 0.85F;
}

void MountedPoseController::ridingMeleeStrike(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mountOnHorse(mount);
  applySwordStrike(mount, attack_phase, false);
}

void MountedPoseController::ridingSpearThrust(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mountOnHorse(mount);
  applySpearThrust(mount, attack_phase);
}

void MountedPoseController::ridingBowShot(const MountedAttachmentFrame &mount,
                                          float draw_phase) {
  mountOnHorse(mount);
  applyBowDraw(mount, draw_phase);
}

void MountedPoseController::ridingShieldDefense(
    const MountedAttachmentFrame &mount, bool raised) {
  mountOnHorse(mount);
  applyShieldDefense(mount, raised);
}

void MountedPoseController::holdReins(const MountedAttachmentFrame &mount,
                                      float left_slack, float right_slack,
                                      float left_tension, float right_tension) {
  mountOnHorse(mount);
  holdReinsImpl(mount, left_slack, right_slack, left_tension, right_tension,
                true, true);
}

void MountedPoseController::holdSpearMounted(
    const MountedAttachmentFrame &mount, SpearGrip grip_style) {
  mountOnHorse(mount);
  applySpearGuard(mount, grip_style);
}

void MountedPoseController::holdBowMounted(
    const MountedAttachmentFrame &mount) {
  mountOnHorse(mount);
  applyBowDraw(mount, 0.0F);
}

void MountedPoseController::applySaddleClearance(
    const MountedAttachmentFrame &mount, const HorseDimensions &dims,
    float forward_bias, float up_bias) {
  float const forward_pull =
      std::max(0.0F, forward_bias) * (dims.bodyWidth * 0.12F) +
      dims.seatForwardOffset * 0.30F;
  float const up_lift =
      std::max(0.0F, up_bias) * (dims.saddleThickness * 0.85F);

  QVector3D const offset = -mount.seat_forward * forward_pull +
                           mount.seat_up * (up_lift + dims.bodyHeight * 0.01F);
  m_pose.pelvis_pos += offset;
  translateUpperBody(offset);
  calculateRidingKnees(mount);
}

void MountedPoseController::stabilizeUpperBody(
    const MountedAttachmentFrame &mount, const HorseDimensions &dims) {
  QVector3D const world_up(0.0F, 1.0F, 0.0F);

  QVector3D right_flat = mount.seat_right;
  right_flat.setY(0.0F);
  if (right_flat.lengthSquared() < 1e-4F) {
    right_flat = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right_flat.normalize();
  }

  QVector3D const shoulder_mid = (m_pose.shoulder_l + m_pose.shoulder_r) * 0.5F;
  QVector3D desired_mid = shoulder_mid;
  desired_mid.setX(m_pose.pelvis_pos.x());
  desired_mid.setZ(m_pose.pelvis_pos.z());

  QVector3D const mid_offset = desired_mid - shoulder_mid;
  m_pose.shoulder_l += mid_offset;
  m_pose.shoulder_r += mid_offset;

  float const desired_half = std::clamp(dims.bodyWidth * 0.44F, 0.10F, 0.32F);
  m_pose.shoulder_l = desired_mid - right_flat * desired_half;
  m_pose.shoulder_r = desired_mid + right_flat * desired_half;

  float const target_neck_height =
      std::max(0.04F, m_pose.neck_base.y() - desired_mid.y());
  m_pose.neck_base = desired_mid + world_up * target_neck_height;

  float const head_height =
      std::max(0.12F, m_pose.head_pos.y() - m_pose.neck_base.y());
  m_pose.head_pos = m_pose.neck_base + world_up * head_height;
}

void MountedPoseController::applyPose(const MountedAttachmentFrame &mount,
                                      const MountedRiderPoseRequest &request) {
  mountOnHorse(mount);
  applySaddleClearance(mount, request.dims, request.clearanceForward,
                       request.clearanceUp);

  float forward = request.forwardBias;
  switch (request.seatPose) {
  case MountedSeatPose::Forward:
    forward += 0.35F;
    break;
  case MountedSeatPose::Defensive:
    forward -= 0.20F;
    break;
  case MountedSeatPose::Neutral:
  default:
    break;
  }
  applyLean(mount, forward, request.sideBias);
  applyTorsoSculpt(mount, request.torsoCompression, request.torsoTwist,
                   request.shoulderDip);
  stabilizeUpperBody(mount, request.dims);

  const bool needs_weapon_right = request.weaponPose != MountedWeaponPose::None;
  const bool needs_weapon_left =
      request.weaponPose == MountedWeaponPose::SpearGuard ||
      request.weaponPose == MountedWeaponPose::SpearThrust ||
      request.weaponPose == MountedWeaponPose::BowDraw;

  const bool shield_claims_left =
      request.shieldPose != MountedShieldPose::None ? true : false;

  bool apply_left_rein =
      request.leftHandOnReins && !shield_claims_left && !needs_weapon_left;
  bool apply_right_rein = request.rightHandOnReins && !needs_weapon_right;

  if (apply_left_rein || apply_right_rein) {
    holdReinsImpl(mount, request.reinSlackLeft, request.reinSlackRight,
                  request.reinTensionLeft, request.reinTensionRight,
                  apply_left_rein, apply_right_rein);
  }

  switch (request.shieldPose) {
  case MountedShieldPose::Guard:
    applyShieldDefense(mount, false);
    break;
  case MountedShieldPose::Raised:
    applyShieldDefense(mount, true);
    break;
  case MountedShieldPose::Stowed:
    applyShieldStowed(mount, request.dims);
    break;
  case MountedShieldPose::None:
  default:
    break;
  }

  switch (request.weaponPose) {
  case MountedWeaponPose::SwordIdle:
    applySwordIdlePose(mount, request.dims);
    break;
  case MountedWeaponPose::SwordStrike:
    applySwordStrike(mount, request.actionPhase,
                     request.shieldPose != MountedShieldPose::None);
    break;
  case MountedWeaponPose::SpearGuard:
    applySpearGuard(mount, SpearGrip::OVERHAND);
    break;
  case MountedWeaponPose::SpearThrust:
    applySpearThrust(mount, request.actionPhase);
    break;
  case MountedWeaponPose::BowDraw:
    applyBowDraw(mount, request.actionPhase);
    break;
  case MountedWeaponPose::None:
  default:
    break;
  }
}

void MountedPoseController::applyLean(const MountedAttachmentFrame &mount,
                                      float forward_lean, float side_lean) {
  float const clamped_forward = std::clamp(forward_lean, -1.0F, 1.0F);
  float const clamped_side = std::clamp(side_lean, -1.0F, 1.0F);

  QVector3D const lean_offset = mount.seat_forward * (clamped_forward * 0.15F) +
                                mount.seat_right * (clamped_side * 0.10F);

  m_pose.shoulder_l += lean_offset;
  m_pose.shoulder_r += lean_offset;
  m_pose.neck_base += lean_offset * 0.9F;
  m_pose.head_pos += lean_offset * 0.60F;
}

void MountedPoseController::applyShieldDefense(
    const MountedAttachmentFrame &mount, bool raised) {
  QVector3D shield_pos = raised ? seatRelative(mount, 0.15F, -0.18F, 0.40F)
                                : seatRelative(mount, 0.05F, -0.16F, 0.22F);
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

void MountedPoseController::applyShieldStowed(
    const MountedAttachmentFrame &mount, const HorseDimensions &dims) {
  QVector3D const rest =
      seatRelative(mount, dims.bodyLength * -0.05F, -dims.bodyWidth * 0.55F,
                   dims.saddleThickness * 0.5F);
  getHand(true) = rest;
  const QVector3D left_outward = computeOutwardDir(true);
  getElbow(true) = solveElbowIK(true, getShoulder(true), rest, left_outward,
                                0.42F, 0.12F, -0.05F, 1.0F);
}

void MountedPoseController::applySwordIdlePose(
    const MountedAttachmentFrame &mount, const HorseDimensions &dims) {
  QVector3D const sword_anchor =
      seatRelative(mount, -dims.bodyLength * 0.12F, dims.bodyWidth * 0.72F,
                   -dims.saddleThickness * 0.60F);
  getHand(false) = sword_anchor;
  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(false) = solveElbowIK(false, getShoulder(false), sword_anchor,
                                 right_outward, 0.42F, 0.10F, -0.06F, 1.0F);
}

void MountedPoseController::applySwordStrike(
    const MountedAttachmentFrame &mount, float attack_phase,
    bool keep_left_hand) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

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

    QVector3D const lean = mount.seat_forward * (0.12F * t);
    m_pose.shoulder_l += lean;
    m_pose.shoulder_r += lean;
    m_pose.neck_base += lean * 0.9F;
    m_pose.head_pos += lean * 0.85F;
  } else {
    float t = (attack_phase - 0.50F) / 0.50F;
    t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_r_target = strike_pos * (1.0F - t) + rest_pos * t;
  }

  getHand(false) = hand_r_target;
  if (!keep_left_hand) {
    getHand(true) = hand_l_target;
  }

  const QVector3D right_outward = computeOutwardDir(false);
  getElbow(false) = solveElbowIK(false, getShoulder(false), hand_r_target,
                                 right_outward, 0.42F, 0.15F, 0.0F, 1.0F);

  if (!keep_left_hand) {
    const QVector3D left_outward = computeOutwardDir(true);
    getElbow(true) = solveElbowIK(true, getShoulder(true), hand_l_target,
                                  left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  }
}

void MountedPoseController::applySpearThrust(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  QVector3D const guard_pos = seatRelative(mount, 0.10F, 0.15F, 0.10F);
  QVector3D const thrust_pos = seatRelative(mount, 0.85F, 0.10F, 0.15F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  if (attack_phase < 0.25F) {
    hand_r_target = guard_pos;
    hand_l_target = guard_pos - mount.seat_right * 0.25F;
  } else if (attack_phase < 0.45F) {
    float t = (attack_phase - 0.25F) / 0.20F;
    t = t * t * t;
    hand_r_target = guard_pos * (1.0F - t) + thrust_pos * t;
    hand_l_target = (guard_pos - mount.seat_right * 0.25F) * (1.0F - t) +
                    (thrust_pos - mount.seat_right * 0.30F) * t;

    QVector3D const lean = mount.seat_forward * (0.18F * t);
    m_pose.shoulder_l += lean;
    m_pose.shoulder_r += lean;
    m_pose.neck_base += lean * 0.9F;
    m_pose.head_pos += lean * 0.85F;
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

void MountedPoseController::applySpearGuard(const MountedAttachmentFrame &mount,
                                            SpearGrip grip_style) {
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

void MountedPoseController::applyBowDraw(const MountedAttachmentFrame &mount,
                                         float draw_phase) {
  draw_phase = std::clamp(draw_phase, 0.0F, 1.0F);

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

void MountedPoseController::applyTorsoSculpt(
    const MountedAttachmentFrame &mount, float compression, float twist,
    float shoulderDip) {
  float const comp = std::clamp(compression, 0.0F, 1.0F);
  float const twist_amt = std::clamp(twist, -1.0F, 1.0F);
  float const dip_amt = std::clamp(shoulderDip, -1.0F, 1.0F);

  if (comp < 1e-3F && std::abs(twist_amt) < 1e-3F &&
      std::abs(dip_amt) < 1e-3F) {
    return;
  }

  QVector3D const forward = mount.seat_forward;
  QVector3D const right = mount.seat_right;
  QVector3D const up = mount.seat_up;

  QVector3D const squeeze = -forward * (0.035F + comp * 0.08F);
  QVector3D const inward = squeeze * comp;
  m_pose.shoulder_l += inward;
  m_pose.shoulder_r += inward;
  m_pose.neck_base += inward * 0.55F;
  m_pose.head_pos += inward * 0.10F;

  QVector3D const chest_lift = up * (0.012F * comp);
  m_pose.neck_base += chest_lift * 0.8F;
  m_pose.head_pos += chest_lift * 0.30F;

  QVector3D const narrow = right * (0.022F * comp);
  m_pose.shoulder_l -= narrow;
  m_pose.shoulder_r += narrow;

  QVector3D const twist_vec = right * (0.03F * twist_amt);
  m_pose.shoulder_l += twist_vec;
  m_pose.shoulder_r -= twist_vec;
  m_pose.neck_base += twist_vec * 0.25F;

  QVector3D const dip_vec = up * (0.03F * dip_amt);
  m_pose.shoulder_l += dip_vec;
  m_pose.shoulder_r -= dip_vec;
}

void MountedPoseController::holdReinsImpl(const MountedAttachmentFrame &mount,
                                          float left_slack, float right_slack,
                                          float left_tension,
                                          float right_tension, bool apply_left,
                                          bool apply_right) {
  left_slack = std::clamp(left_slack, 0.0F, 1.0F);
  right_slack = std::clamp(right_slack, 0.0F, 1.0F);
  left_tension = std::clamp(left_tension, 0.0F, 1.0F);
  right_tension = std::clamp(right_tension, 0.0F, 1.0F);

  if (apply_left) {
    QVector3D const left_rein_pos =
        reinAnchor(mount, true, left_slack, left_tension);
    getHand(true) = left_rein_pos;
    const QVector3D left_outward = computeOutwardDir(true);
    getElbow(true) = solveElbowIK(true, getShoulder(true), left_rein_pos,
                                  left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  }

  if (apply_right) {
    QVector3D const right_rein_pos =
        reinAnchor(mount, false, right_slack, right_tension);
    getHand(false) = right_rein_pos;
    const QVector3D right_outward = computeOutwardDir(false);
    getElbow(false) = solveElbowIK(false, getShoulder(false), right_rein_pos,
                                   right_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  }
}

void MountedPoseController::attachFeetToStirrups(
    const MountedAttachmentFrame &mount) {

  m_pose.foot_l = mount.stirrup_bottom_left + mount.ground_offset;
  m_pose.foot_r = mount.stirrup_bottom_right + mount.ground_offset;
}

void MountedPoseController::positionPelvisOnSaddle(
    const MountedAttachmentFrame &mount) {
  QVector3D const seat_world = mount.seat_position + mount.ground_offset;
  QVector3D const delta = seat_world - m_pose.pelvis_pos;
  m_pose.pelvis_pos = seat_world;
  translateUpperBody(delta);
}

void MountedPoseController::translateUpperBody(const QVector3D &delta) {
  m_pose.shoulder_l += delta;
  m_pose.shoulder_r += delta;
  m_pose.neck_base += delta;
  m_pose.head_pos += delta;
  m_pose.elbow_l += delta;
  m_pose.elbow_r += delta;
  m_pose.hand_l += delta;
  m_pose.hand_r += delta;
}

void MountedPoseController::calculateRidingKnees(
    const MountedAttachmentFrame &mount) {

  QVector3D const hip_offset = mount.seat_up * -0.02F;
  QVector3D const hip_left =
      m_pose.pelvis_pos - mount.seat_right * 0.10F + hip_offset;
  QVector3D const hip_right =
      m_pose.pelvis_pos + mount.seat_right * 0.10F + hip_offset;

  float const height_scale = m_anim_ctx.variation.height_scale;

  m_pose.knee_l = solveKneeIK(true, hip_left, m_pose.foot_l, height_scale);
  m_pose.knee_r = solveKneeIK(false, hip_right, m_pose.foot_r, height_scale);
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
                                        float height_scale) const -> QVector3D {
  using HP = HumanProportions;

  QVector3D hip_to_foot = foot - hip;
  float const distance = hip_to_foot.length();
  if (distance < 1e-5F) {
    return hip;
  }

  float const upper_len = HP::UPPER_LEG_LEN * height_scale;
  float const lower_len = HP::LOWER_LEG_LEN * height_scale;
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

  float const knee_floor = HP::GROUND_Y + m_pose.foot_y_offset * 0.5F;
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
  return is_left ? m_pose.shoulder_l : m_pose.shoulder_r;
}

auto MountedPoseController::getHand(bool is_left) -> QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto MountedPoseController::getHand(bool is_left) const -> const QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto MountedPoseController::getElbow(bool is_left) -> QVector3D & {
  return is_left ? m_pose.elbow_l : m_pose.elbow_r;
}

auto MountedPoseController::computeRightAxis() const -> QVector3D {
  QVector3D right_axis = m_pose.shoulder_r - m_pose.shoulder_l;
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
