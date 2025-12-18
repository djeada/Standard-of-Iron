#include "mounted_pose_controller.h"
#include "../horse/rig.h"
#include "humanoid_math.h"
#include "humanoid_specs.h"
#include "pose_controller.h"
#include <QDebug>
#include <QString>
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

void MountedPoseController::mount_on_horse(
    const MountedAttachmentFrame &mount) {
  positionPelvisOnSaddle(mount);
  attach_feet_to_stirrups(mount);
  calculate_riding_knees(mount);
}

void MountedPoseController::dismount() {

  using HP = HumanProportions;
  m_pose.pelvis_pos = QVector3D(0.0F, HP::WAIST_Y, 0.0F);
  m_pose.foot_l = QVector3D(-0.14F, HP::GROUND_Y + m_pose.foot_y_offset, 0.06F);
  m_pose.foot_r = QVector3D(0.14F, HP::GROUND_Y + m_pose.foot_y_offset, -0.06F);
}

void MountedPoseController::ridingIdle(const MountedAttachmentFrame &mount) {
  mount_on_horse(mount);

  QVector3D const left_hand_rest = seatRelative(mount, 0.12F, -0.14F, -0.05F);
  QVector3D const right_hand_rest = seatRelative(mount, 0.12F, 0.14F, -0.05F);

  get_hand(true) = left_hand_rest;
  get_hand(false) = right_hand_rest;

  const QVector3D left_outward = compute_outward_dir(true);
  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), left_hand_rest,
                                   left_outward, 0.45F, 0.12F, -0.05F, 1.0F);
  get_elbow(false) = solve_elbow_ik(false, get_shoulder(false), right_hand_rest,
                                    right_outward, 0.45F, 0.12F, -0.05F, 1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, "ridingIdle");
}

void MountedPoseController::ridingLeaning(const MountedAttachmentFrame &mount,
                                          float forward_lean, float side_lean) {
  mount_on_horse(mount);
  apply_lean(mount, forward_lean, side_lean);
}

void MountedPoseController::ridingCharging(const MountedAttachmentFrame &mount,
                                           float intensity) {
  intensity = std::clamp(intensity, 0.0F, 1.0F);

  mount_on_horse(mount);

  QVector3D const charge_lean = mount.seat_forward * (0.25F * intensity);
  m_pose.shoulder_l += charge_lean;
  m_pose.shoulder_r += charge_lean;
  m_pose.neck_base += charge_lean * 0.85F;

  float const crouch = 0.08F * intensity;
  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - crouch);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - crouch);
  m_pose.neck_base.setY(m_pose.neck_base.y() - crouch * 0.8F);

  update_head_hierarchy(mount, 0.0F, 0.0F, "ridingCharging");

  holdReins(mount, 0.2F, 0.2F, 0.85F, 0.85F);
}

void MountedPoseController::ridingReining(const MountedAttachmentFrame &mount,
                                          float left_tension,
                                          float right_tension) {
  left_tension = std::clamp(left_tension, 0.0F, 1.0F);
  right_tension = std::clamp(right_tension, 0.0F, 1.0F);

  mount_on_horse(mount);

  QVector3D left_rein_pos = reinAnchor(mount, true, 0.15F, left_tension);
  QVector3D right_rein_pos = reinAnchor(mount, false, 0.15F, right_tension);

  get_hand(true) = left_rein_pos;
  get_hand(false) = right_rein_pos;

  const QVector3D left_outward = compute_outward_dir(true);
  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), left_rein_pos,
                                   left_outward, 0.52F, 0.08F, -0.12F, 1.0F);
  get_elbow(false) = solve_elbow_ik(false, get_shoulder(false), right_rein_pos,
                                    right_outward, 0.52F, 0.08F, -0.12F, 1.0F);

  float const avg_tension = (left_tension + right_tension) * 0.5F;
  QVector3D const lean_back = mount.seat_forward * (-0.08F * avg_tension);
  m_pose.shoulder_l += lean_back;
  m_pose.shoulder_r += lean_back;
  m_pose.neck_base += lean_back * 0.9F;

  update_head_hierarchy(mount, 0.0F, 0.0F, "ridingReining");
}

void MountedPoseController::ridingMeleeStrike(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mount_on_horse(mount);
  apply_sword_strike(mount, attack_phase, false);
}

void MountedPoseController::ridingSpearThrust(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mount_on_horse(mount);
  apply_spear_thrust(mount, attack_phase);
}

void MountedPoseController::ridingBowShot(const MountedAttachmentFrame &mount,
                                          float draw_phase) {
  mount_on_horse(mount);
  apply_bow_draw(mount, draw_phase);
}

void MountedPoseController::ridingShieldDefense(
    const MountedAttachmentFrame &mount, bool raised) {
  mount_on_horse(mount);
  apply_shield_defense(mount, raised);
}

void MountedPoseController::holdReins(const MountedAttachmentFrame &mount,
                                      float left_slack, float right_slack,
                                      float left_tension, float right_tension) {
  mount_on_horse(mount);
  holdReinsImpl(mount, left_slack, right_slack, left_tension, right_tension,
                true, true);
}

void MountedPoseController::holdSpearMounted(
    const MountedAttachmentFrame &mount, SpearGrip grip_style) {
  mount_on_horse(mount);
  apply_spear_guard(mount, grip_style);
}

void MountedPoseController::holdBowMounted(
    const MountedAttachmentFrame &mount) {
  mount_on_horse(mount);
  apply_bow_draw(mount, 0.0F);
}

void MountedPoseController::apply_saddle_clearance(
    const MountedAttachmentFrame &mount, const HorseDimensions &dims,
    float forward_bias, float up_bias) {
  float const forward_pull =
      std::max(0.0F, forward_bias) * (dims.body_width * 0.12F) +
      dims.seat_forward_offset * 0.30F;
  float const up_lift =
      std::max(0.0F, up_bias) * (dims.saddle_thickness * 0.85F);

  QVector3D const offset = -mount.seat_forward * forward_pull +
                           mount.seat_up * (up_lift + dims.body_height * 0.01F);
  m_pose.pelvis_pos += offset;
  translate_upper_body(offset);
  calculate_riding_knees(mount);
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

  float const desired_half = std::clamp(dims.body_width * 0.44F, 0.10F, 0.32F);
  m_pose.shoulder_l = desired_mid - right_flat * desired_half;
  m_pose.shoulder_r = desired_mid + right_flat * desired_half;

  float const target_neck_height =
      std::max(0.04F, m_pose.neck_base.y() - desired_mid.y());
  m_pose.neck_base = desired_mid + world_up * target_neck_height;

  float const head_height =
      std::max(0.12F, m_pose.head_pos.y() - m_pose.neck_base.y());
  m_pose.head_pos = m_pose.neck_base + world_up * head_height;
}

void MountedPoseController::apply_pose(const MountedAttachmentFrame &mount,
                                       const MountedRiderPoseRequest &request) {
  mount_on_horse(mount);
  apply_saddle_clearance(mount, request.dims, request.clearanceForward,
                         request.clearanceUp);

  stabilizeUpperBody(mount, request.dims);

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
  apply_lean(mount, forward, request.sideBias);

  apply_torso_sculpt(mount, request.torsoCompression, request.torsoTwist,
                     request.shoulderDip);

  float const clamped_forward = std::clamp(forward, -1.0F, 1.0F);
  float const clamped_side = std::clamp(request.sideBias, -1.0F, 1.0F);
  update_head_hierarchy(mount, clamped_forward * 0.4F, clamped_side * 0.4F,
                        "applyPose_fixup");

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
    apply_shield_defense(mount, false);
    break;
  case MountedShieldPose::Raised:
    apply_shield_defense(mount, true);
    break;
  case MountedShieldPose::Stowed:
    apply_shield_stowed(mount, request.dims);
    break;
  case MountedShieldPose::None:
  default:
    break;
  }

  switch (request.weaponPose) {
  case MountedWeaponPose::SwordIdle:
    apply_sword_idle_pose(mount, request.dims);
    break;
  case MountedWeaponPose::SwordStrike:
    apply_sword_strike(mount, request.actionPhase,
                       request.shieldPose != MountedShieldPose::None);
    break;
  case MountedWeaponPose::SpearGuard:
    apply_spear_guard(mount, SpearGrip::OVERHAND);
    break;
  case MountedWeaponPose::SpearThrust:
    apply_spear_thrust(mount, request.actionPhase);
    break;
  case MountedWeaponPose::BowDraw:
    apply_bow_draw(mount, request.actionPhase);
    break;
  case MountedWeaponPose::None:
  default:
    break;
  }
}

void MountedPoseController::apply_lean(const MountedAttachmentFrame &mount,
                                       float forward_lean, float side_lean) {
  float const clamped_forward = std::clamp(forward_lean, -1.0F, 1.0F);
  float const clamped_side = std::clamp(side_lean, -1.0F, 1.0F);

  QVector3D const lean_offset = mount.seat_forward * (clamped_forward * 0.15F) +
                                mount.seat_right * (clamped_side * 0.10F);

  m_pose.shoulder_l += lean_offset;
  m_pose.shoulder_r += lean_offset;
  m_pose.neck_base += lean_offset * 0.9F;

  update_head_hierarchy(mount, clamped_forward * 0.4F, clamped_side * 0.4F,
                        "apply_lean");
}

void MountedPoseController::apply_shield_defense(
    const MountedAttachmentFrame &mount, bool raised) {
  QVector3D shield_pos = raised ? seatRelative(mount, 0.15F, -0.18F, 0.40F)
                                : seatRelative(mount, 0.05F, -0.16F, 0.22F);
  float const rein_slack = raised ? 0.15F : 0.30F;
  float const rein_tension = raised ? 0.45F : 0.25F;
  QVector3D const rein_pos = reinAnchor(mount, false, rein_slack, rein_tension);

  get_hand(true) = shield_pos;
  get_hand(false) = rein_pos;

  const QVector3D left_outward = compute_outward_dir(true);
  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), shield_pos,
                                   left_outward, 0.45F, 0.15F, -0.10F, 1.0F);
  get_elbow(false) = solve_elbow_ik(false, get_shoulder(false), rein_pos,
                                    right_outward, 0.45F, 0.12F, -0.08F, 1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, "shield_defense");
}

void MountedPoseController::apply_shield_stowed(
    const MountedAttachmentFrame &mount, const HorseDimensions &dims) {
  QVector3D const rest =
      seatRelative(mount, dims.body_length * -0.05F, -dims.body_width * 0.55F,
                   dims.saddle_thickness * 0.5F);
  get_hand(true) = rest;
  const QVector3D left_outward = compute_outward_dir(true);
  get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), rest, left_outward,
                                   0.42F, 0.12F, -0.05F, 1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, "shield_stowed");
}

void MountedPoseController::apply_sword_idle_pose(
    const MountedAttachmentFrame &mount, const HorseDimensions &dims) {
  QVector3D const shoulder_r = get_shoulder(false);
  QVector3D const sword_anchor =
      shoulder_r + mount.seat_right * (dims.body_width * 0.90F) +
      mount.seat_forward * (dims.body_length * 0.22F) +
      mount.seat_up *
          (dims.body_height * 0.06F + dims.saddle_thickness * 0.10F);

  get_hand(false) = sword_anchor;
  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(false) = solve_elbow_ik(false, shoulder_r, sword_anchor,
                                    right_outward, 0.46F, 0.24F, -0.05F, 1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, "sword_idle");
}

void MountedPoseController::apply_sword_strike(
    const MountedAttachmentFrame &mount, float attack_phase,
    bool keep_left_hand) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  QVector3D const rest_pos = seatRelative(mount, 0.08F, 0.20F, 0.12F);
  QVector3D const chamber_pos = seatRelative(mount, -0.05F, 0.25F, 0.40F);
  QVector3D const apex_pos = seatRelative(mount, -0.02F, 0.30F, 0.48F);
  QVector3D const strike_pos = seatRelative(mount, 0.45F, 0.35F, 0.0F);
  QVector3D const followthrough_pos = seatRelative(mount, 0.55F, 0.25F, -0.10F);

  QVector3D hand_r_target;
  QVector3D hand_l_target =
      reinAnchor(mount, true, 0.20F, 0.25F) + mount.seat_up * -0.02F;

  float torso_twist = 0.0F;
  float side_lean = 0.0F;
  float forward_lean = 0.0F;
  float shoulder_dip = 0.0F;

  if (attack_phase < 0.18F) {

    float t = attack_phase / 0.18F;
    float ease_t = t * t;
    hand_r_target = rest_pos * (1.0F - ease_t) + chamber_pos * ease_t;

    torso_twist = -0.04F * ease_t;
    shoulder_dip = 0.03F * ease_t;

    update_head_hierarchy(mount, 0.0F, 0.0F, "sword_chamber");
  } else if (attack_phase < 0.28F) {

    float t = (attack_phase - 0.18F) / 0.10F;
    float ease_t = t * t * (3.0F - 2.0F * t);
    hand_r_target = chamber_pos * (1.0F - ease_t) + apex_pos * ease_t;

    torso_twist = -0.04F;
    shoulder_dip = 0.03F + 0.02F * ease_t;

    update_head_hierarchy(mount, 0.0F, 0.0F, "sword_apex");
  } else if (attack_phase < 0.48F) {

    float t = (attack_phase - 0.28F) / 0.20F;
    float power_t = t * t * t;
    hand_r_target = apex_pos * (1.0F - power_t) + strike_pos * power_t;

    torso_twist = -0.04F + 0.12F * power_t;
    side_lean = 0.08F * power_t;
    forward_lean = 0.06F * power_t;
    shoulder_dip = 0.05F - 0.08F * power_t;

    hand_l_target += mount.seat_up * (-0.03F * power_t);

    update_head_hierarchy(mount, 0.3F * power_t, 0.2F * power_t,
                          "sword_strike");
  } else if (attack_phase < 0.65F) {

    float t = (attack_phase - 0.48F) / 0.17F;
    float ease_t = t * t * (3.0F - 2.0F * t);
    hand_r_target = strike_pos * (1.0F - ease_t) + followthrough_pos * ease_t;

    torso_twist = 0.08F - 0.02F * t;
    side_lean = 0.08F - 0.03F * t;
    forward_lean = 0.06F - 0.02F * t;
    shoulder_dip = -0.03F;

    update_head_hierarchy(mount, 0.15F, 0.1F, "sword_followthrough");
  } else {

    float t = (attack_phase - 0.65F) / 0.35F;
    float ease_t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_r_target = followthrough_pos * (1.0F - ease_t) + rest_pos * ease_t;

    torso_twist = 0.06F * (1.0F - ease_t);
    side_lean = 0.05F * (1.0F - ease_t);
    forward_lean = 0.04F * (1.0F - ease_t);
    shoulder_dip = -0.03F * (1.0F - ease_t);

    update_head_hierarchy(mount, 0.0F, 0.0F, "sword_recover");
  }

  if (std::abs(torso_twist) > 0.001F) {
    QVector3D const twist_offset = mount.seat_forward * torso_twist;
    m_pose.shoulder_r += twist_offset;
    m_pose.shoulder_l -= twist_offset * 0.5F;
  }

  if (side_lean > 0.001F) {
    QVector3D const lean_offset = mount.seat_right * side_lean;
    m_pose.shoulder_l += lean_offset;
    m_pose.shoulder_r += lean_offset;
    m_pose.neck_base += lean_offset * 0.8F;
  }

  if (forward_lean > 0.001F) {
    QVector3D const lean_offset = mount.seat_forward * forward_lean;
    m_pose.shoulder_l += lean_offset;
    m_pose.shoulder_r += lean_offset;
    m_pose.neck_base += lean_offset * 0.9F;
  }

  if (std::abs(shoulder_dip) > 0.001F) {
    m_pose.shoulder_r += mount.seat_up * shoulder_dip;
  }

  get_hand(false) = hand_r_target;
  if (!keep_left_hand) {
    get_hand(true) = hand_l_target;
  }

  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(false) = solve_elbow_ik(false, get_shoulder(false), hand_r_target,
                                    right_outward, 0.42F, 0.15F, 0.0F, 1.0F);

  if (!keep_left_hand) {
    const QVector3D left_outward = compute_outward_dir(true);
    get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), hand_l_target,
                                     left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  }
}

void MountedPoseController::apply_spear_thrust(
    const MountedAttachmentFrame &mount, float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  QVector3D const guard_pos = seatRelative(mount, 0.12F, 0.15F, 0.15F);
  QVector3D const couch_pos = seatRelative(mount, 0.05F, 0.12F, 0.08F);
  QVector3D const thrust_pos = seatRelative(mount, 0.95F, 0.08F, 0.18F);
  QVector3D const extended_pos = seatRelative(mount, 1.05F, 0.05F, 0.15F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  float forward_lean = 0.0F;
  float torso_twist = 0.0F;
  float shoulder_drop = 0.0F;
  float torso_compression = 0.0F;

  if (attack_phase < 0.20F) {

    float t = attack_phase / 0.20F;
    float ease_t = t * t;
    hand_r_target = guard_pos * (1.0F - ease_t) + couch_pos * ease_t;
    hand_l_target = guard_pos - mount.seat_right * 0.25F +
                    (couch_pos - guard_pos) * ease_t * 0.6F;

    torso_compression = 0.03F * ease_t;
    forward_lean = 0.04F * ease_t;

    update_head_hierarchy(mount, 0.1F * ease_t, 0.0F, "spear_couch");
  } else if (attack_phase < 0.30F) {

    hand_r_target = couch_pos;
    hand_l_target = couch_pos - mount.seat_right * 0.22F;

    torso_compression = 0.03F;
    forward_lean = 0.04F;

    update_head_hierarchy(mount, 0.1F, 0.0F, "spear_tension");
  } else if (attack_phase < 0.50F) {

    float t = (attack_phase - 0.30F) / 0.20F;
    float power_t = t * t * t;
    hand_r_target = couch_pos * (1.0F - power_t) + thrust_pos * power_t;
    hand_l_target = (couch_pos - mount.seat_right * 0.22F) * (1.0F - power_t) +
                    (thrust_pos - mount.seat_right * 0.28F) * power_t;

    forward_lean = 0.04F + 0.16F * power_t;
    torso_twist = 0.05F * power_t;
    shoulder_drop = 0.04F * power_t;
    torso_compression = 0.03F * (1.0F - power_t * 0.5F);

    update_head_hierarchy(mount, 0.5F * power_t, 0.0F, "spear_thrust");
  } else if (attack_phase < 0.65F) {

    float t = (attack_phase - 0.50F) / 0.15F;
    float ease_t = t * t * (3.0F - 2.0F * t);
    hand_r_target = thrust_pos * (1.0F - ease_t) + extended_pos * ease_t;
    hand_l_target = (thrust_pos - mount.seat_right * 0.28F) * (1.0F - ease_t) +
                    (extended_pos - mount.seat_right * 0.32F) * ease_t;

    forward_lean = 0.20F;
    torso_twist = 0.05F;
    shoulder_drop = 0.04F;

    update_head_hierarchy(mount, 0.5F, 0.0F, "spear_extend");
  } else {

    float t = (attack_phase - 0.65F) / 0.35F;
    float ease_t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_r_target = extended_pos * (1.0F - ease_t) + guard_pos * ease_t;
    hand_l_target =
        (extended_pos - mount.seat_right * 0.32F) * (1.0F - ease_t) +
        (guard_pos - mount.seat_right * 0.25F) * ease_t;

    forward_lean = 0.20F * (1.0F - ease_t);
    torso_twist = 0.05F * (1.0F - ease_t);
    shoulder_drop = 0.04F * (1.0F - ease_t);

    update_head_hierarchy(mount, 0.0F, 0.0F, "spear_recover");
  }

  if (forward_lean > 0.001F) {
    QVector3D const lean_offset = mount.seat_forward * forward_lean;
    m_pose.shoulder_l += lean_offset;
    m_pose.shoulder_r += lean_offset;
    m_pose.neck_base += lean_offset * 0.85F;
  }

  if (std::abs(torso_twist) > 0.001F) {
    QVector3D const twist_offset = mount.seat_forward * torso_twist;
    m_pose.shoulder_r += twist_offset;
    m_pose.shoulder_l -= twist_offset * 0.3F;
  }

  if (shoulder_drop > 0.001F) {
    m_pose.shoulder_r -= mount.seat_up * shoulder_drop;
    m_pose.shoulder_l -= mount.seat_up * (shoulder_drop * 0.3F);
  }

  if (torso_compression > 0.001F) {
    m_pose.shoulder_l -= mount.seat_up * torso_compression;
    m_pose.shoulder_r -= mount.seat_up * torso_compression;
    m_pose.neck_base -= mount.seat_up * (torso_compression * 0.6F);
  }

  get_hand(false) = hand_r_target;
  get_hand(true) = hand_l_target;

  const QVector3D left_outward = compute_outward_dir(true);
  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), hand_l_target,
                                   left_outward, 0.48F, 0.10F, -0.06F, 1.0F);
  get_elbow(false) = solve_elbow_ik(false, get_shoulder(false), hand_r_target,
                                    right_outward, 0.48F, 0.10F, -0.04F, 1.0F);
}

void MountedPoseController::apply_spear_guard(
    const MountedAttachmentFrame &mount, SpearGrip grip_style) {
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

  get_hand(false) = hand_r_target;
  get_hand(true) = hand_l_target;

  const QVector3D left_outward = compute_outward_dir(true);
  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), hand_l_target,
                                   left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  get_elbow(false) = solve_elbow_ik(false, get_shoulder(false), hand_r_target,
                                    right_outward, 0.45F, 0.12F, -0.05F, 1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, "spear_guard_pose");
}

void MountedPoseController::apply_bow_draw(const MountedAttachmentFrame &mount,
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

  get_hand(true) = hand_l_target;
  get_hand(false) = hand_r_target;

  const QVector3D left_outward = compute_outward_dir(true);
  const QVector3D right_outward = compute_outward_dir(false);
  get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), hand_l_target,
                                   left_outward, 0.50F, 0.08F, -0.05F, 1.0F);
  get_elbow(false) = solve_elbow_ik(false, get_shoulder(false), hand_r_target,
                                    right_outward, 0.48F, 0.12F, -0.08F, 1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, "bow_draw");
}

void MountedPoseController::apply_torso_sculpt(
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
  m_pose.head_pos += inward * 0.55F;

  QVector3D const chest_lift = up * (0.012F * comp);
  m_pose.neck_base += chest_lift * 0.8F;
  m_pose.head_pos += chest_lift * 0.8F;

  QVector3D const narrow = right * (0.022F * comp);
  m_pose.shoulder_l -= narrow;
  m_pose.shoulder_r += narrow;

  // Twist is visually very sensitive because it changes the shoulder line
  // (and thus the derived torso frame). Keep it extremely small.
  QVector3D const twist_vec = right * (0.0003F * twist_amt);
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
    get_hand(true) = left_rein_pos;
    const QVector3D left_outward = compute_outward_dir(true);
    get_elbow(true) = solve_elbow_ik(true, get_shoulder(true), left_rein_pos,
                                     left_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  }

  if (apply_right) {
    QVector3D const right_rein_pos =
        reinAnchor(mount, false, right_slack, right_tension);
    get_hand(false) = right_rein_pos;
    const QVector3D right_outward = compute_outward_dir(false);
    get_elbow(false) =
        solve_elbow_ik(false, get_shoulder(false), right_rein_pos,
                       right_outward, 0.45F, 0.12F, -0.08F, 1.0F);
  }
}

void MountedPoseController::attach_feet_to_stirrups(
    const MountedAttachmentFrame &mount) {

  m_pose.foot_l = mount.stirrup_bottom_left + mount.ground_offset;
  m_pose.foot_r = mount.stirrup_bottom_right + mount.ground_offset;
}

void MountedPoseController::positionPelvisOnSaddle(
    const MountedAttachmentFrame &mount) {
  QVector3D const seat_world = mount.seat_position + mount.ground_offset;
  QVector3D const delta = seat_world - m_pose.pelvis_pos;
  m_pose.pelvis_pos = seat_world;
  translate_upper_body(delta);
}

void MountedPoseController::translate_upper_body(const QVector3D &delta) {
  m_pose.shoulder_l += delta;
  m_pose.shoulder_r += delta;
  m_pose.neck_base += delta;
  m_pose.head_pos += delta;
  m_pose.elbow_l += delta;
  m_pose.elbow_r += delta;
  m_pose.hand_l += delta;
  m_pose.hand_r += delta;
}

void MountedPoseController::calculate_riding_knees(
    const MountedAttachmentFrame &mount) {

  QVector3D const hip_offset = mount.seat_up * -0.02F;
  QVector3D const hip_left =
      m_pose.pelvis_pos - mount.seat_right * 0.10F + hip_offset;
  QVector3D const hip_right =
      m_pose.pelvis_pos + mount.seat_right * 0.10F + hip_offset;

  float const height_scale = m_anim_ctx.variation.height_scale;

  m_pose.knee_l = solve_knee_ik(true, hip_left, m_pose.foot_l, height_scale);
  m_pose.knee_r = solve_knee_ik(false, hip_right, m_pose.foot_r, height_scale);
}

auto MountedPoseController::solve_elbow_ik(
    bool, const QVector3D &shoulder, const QVector3D &hand,
    const QVector3D &outward_dir, float along_frac, float lateral_offset,
    float y_bias, float outward_sign) const -> QVector3D {
  return elbow_bend_torso(shoulder, hand, outward_dir, along_frac,
                          lateral_offset, y_bias, outward_sign);
}

auto MountedPoseController::solve_knee_ik(
    bool is_left, const QVector3D &hip, const QVector3D &foot,
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

auto MountedPoseController::get_shoulder(bool is_left) const
    -> const QVector3D & {
  return is_left ? m_pose.shoulder_l : m_pose.shoulder_r;
}

auto MountedPoseController::get_hand(bool is_left) -> QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto MountedPoseController::get_hand(bool is_left) const -> const QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto MountedPoseController::get_elbow(bool is_left) -> QVector3D & {
  return is_left ? m_pose.elbow_l : m_pose.elbow_r;
}

auto MountedPoseController::compute_right_axis() const -> QVector3D {
  QVector3D right_axis = m_pose.shoulder_r - m_pose.shoulder_l;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right_axis.normalize();
  return right_axis;
}

auto MountedPoseController::compute_outward_dir(bool is_left) const
    -> QVector3D {
  QVector3D const right_axis = compute_right_axis();
  return is_left ? -right_axis : right_axis;
}

void MountedPoseController::apply_fixed_head_frame(
    const MountedAttachmentFrame &mount, std::string_view debug_label) {
  using HP = HumanProportions;
  float const h_scale = m_anim_ctx.variation.height_scale;
  float const neck_len = (HP::HEAD_HEIGHT * 0.5F + 0.045F) * h_scale;

  QVector3D up_dir = mount.seat_up;
  if (up_dir.lengthSquared() < 1e-6F) {
    up_dir = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    up_dir.normalize();
  }

  QVector3D fwd_dir = mount.seat_forward;
  if (fwd_dir.lengthSquared() < 1e-6F) {
    fwd_dir = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    fwd_dir.normalize();
  }

  QVector3D right_dir = QVector3D::crossProduct(fwd_dir, up_dir);
  if (right_dir.lengthSquared() < 1e-6F) {
    right_dir = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right_dir.normalize();
  }
  fwd_dir = QVector3D::crossProduct(up_dir, right_dir).normalized();

  m_pose.head_pos = m_pose.neck_base + up_dir * neck_len;

  QVector3D prev_origin = m_pose.head_frame.origin;
  QVector3D prev_up = m_pose.head_frame.up;
  QVector3D prev_forward = m_pose.head_frame.forward;

  m_pose.head_frame.origin = m_pose.head_pos;
  m_pose.head_frame.up = up_dir;
  m_pose.head_frame.right = right_dir;
  m_pose.head_frame.forward = fwd_dir;
  if (m_pose.head_r < 0.01F) {
    m_pose.head_r = 0.12F;
  }
  m_pose.head_frame.radius = m_pose.head_r;
  m_pose.body_frames.head = m_pose.head_frame;
}

void MountedPoseController::update_head_hierarchy(
    const MountedAttachmentFrame &mount, float extra_forward_tilt,
    float extra_side_tilt, std::string_view debug_label) {
  (void)extra_forward_tilt;
  (void)extra_side_tilt;
  apply_fixed_head_frame(mount, debug_label);
}

void MountedPoseController::finalize_head_sync(
    const MountedAttachmentFrame &mount, std::string_view debug_label) {
  apply_fixed_head_frame(mount, debug_label);
}

} // namespace Render::GL
