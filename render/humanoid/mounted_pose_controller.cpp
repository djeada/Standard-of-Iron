#include "mounted_pose_controller.h"

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "../horse/horse_motion.h"
#include "../horse/horse_renderer_base.h"
#include "animation/attack_pose_manifest.h"
#include "animation/mounted_pose_manifest.h"
#include "animation/posture_pose_manifest.h"
#include "humanoid_math.h"
#include "humanoid_specs.h"
#include "pose_controller.h"
#include "pose_primitives.h"

namespace Render::GL {

namespace {

auto seat_relative(const MountedAttachmentFrame& mount,
                   float forward,
                   float right,
                   float up) -> QVector3D {
  QVector3D const base = mount.seat_position;
  return base + mount.seat_forward * forward + mount.seat_right * right +
         mount.seat_up * up;
}

auto seat_relative(const MountedAttachmentFrame& mount,
                   Animation::MountedSeatOffset offset) -> QVector3D {
  return seat_relative(mount, offset.forward, offset.right, offset.up);
}

auto rein_anchor(const MountedAttachmentFrame& mount,
                 Side side,
                 float slack,
                 float tension) -> QVector3D {
  return compute_rein_handle(mount, side, slack, tension);
}

auto mounted_axis_delta(const MountedAttachmentFrame& mount,
                        Animation::MountedAxisDelta delta) -> QVector3D {
  return mount.seat_forward * delta.forward + mount.seat_right * delta.right +
         mount.seat_up * delta.up + QVector3D(0.0F, delta.world_y, 0.0F);
}

auto mounted_offset(const MountedAttachmentFrame& mount,
                    Animation::MountedSeatOffset offset) -> QVector3D {
  return mount.seat_forward * offset.forward + mount.seat_right * offset.right +
         mount.seat_up * offset.up;
}

auto mounted_hand_anchor(const HumanoidPose& pose,
                         const MountedAttachmentFrame& mount,
                         Animation::MountedHandAnchor anchor) -> QVector3D {
  switch (anchor) {
  case Animation::MountedHandAnchor::LeftShoulder:
    return pose.shoulder_l;
  case Animation::MountedHandAnchor::RightShoulder:
    return pose.shoulder_r;
  case Animation::MountedHandAnchor::Seat:
    break;
  }
  return mount.seat_position;
}

void apply_mounted_posture_sample(HumanoidPose& pose,
                                  const MountedAttachmentFrame& mount,
                                  const Animation::MountedRiderPostureSample& sample) {
  if (!sample.active) {
    return;
  }
  pose.shoulder_l += mounted_axis_delta(mount, sample.shoulder_l_delta);
  pose.shoulder_r += mounted_axis_delta(mount, sample.shoulder_r_delta);
  pose.neck_base += mounted_axis_delta(mount, sample.neck_delta);
  pose.head_pos += mounted_axis_delta(mount, sample.head_delta);
}

auto to_animation_spear_grip(SpearGrip grip) -> Animation::MountedSpearGuardGrip {
  switch (grip) {
  case SpearGrip::COUCHED:
    return Animation::MountedSpearGuardGrip::Couched;
  case SpearGrip::TWO_HANDED:
    return Animation::MountedSpearGuardGrip::TwoHanded;
  case SpearGrip::OVERHAND:
    break;
  }
  return Animation::MountedSpearGuardGrip::Overhand;
}

auto to_animation_seat_pose(MountedPoseController::MountedSeatPose pose)
    -> Animation::MountedSeatPoseKind {
  switch (pose) {
  case MountedPoseController::MountedSeatPose::Forward:
    return Animation::MountedSeatPoseKind::Forward;
  case MountedPoseController::MountedSeatPose::Defensive:
    return Animation::MountedSeatPoseKind::Defensive;
  case MountedPoseController::MountedSeatPose::Neutral:
    break;
  }
  return Animation::MountedSeatPoseKind::Neutral;
}

auto to_animation_weapon_pose(MountedPoseController::MountedWeaponPose pose)
    -> Animation::MountedWeaponPoseKind {
  switch (pose) {
  case MountedPoseController::MountedWeaponPose::SwordIdle:
    return Animation::MountedWeaponPoseKind::SwordIdle;
  case MountedPoseController::MountedWeaponPose::SwordStrike:
    return Animation::MountedWeaponPoseKind::SwordStrike;
  case MountedPoseController::MountedWeaponPose::SpearGuard:
    return Animation::MountedWeaponPoseKind::SpearGuard;
  case MountedPoseController::MountedWeaponPose::SpearThrust:
    return Animation::MountedWeaponPoseKind::SpearThrust;
  case MountedPoseController::MountedWeaponPose::BowDraw:
    return Animation::MountedWeaponPoseKind::BowDraw;
  case MountedPoseController::MountedWeaponPose::None:
    break;
  }
  return Animation::MountedWeaponPoseKind::None;
}

auto to_animation_shield_pose(MountedPoseController::MountedShieldPose pose)
    -> Animation::MountedShieldPoseKind {
  switch (pose) {
  case MountedPoseController::MountedShieldPose::Stowed:
    return Animation::MountedShieldPoseKind::Stowed;
  case MountedPoseController::MountedShieldPose::Guard:
    return Animation::MountedShieldPoseKind::Guard;
  case MountedPoseController::MountedShieldPose::Raised:
    return Animation::MountedShieldPoseKind::Raised;
  case MountedPoseController::MountedShieldPose::None:
    break;
  }
  return Animation::MountedShieldPoseKind::None;
}

} // namespace

MountedPoseController::MountedPoseController(HumanoidPose& pose,
                                             const HumanoidAnimationContext& anim_ctx)
    : m_pose(pose)
    , m_anim_ctx(anim_ctx) {
}

void MountedPoseController::mount_on_horse(const MountedAttachmentFrame& mount) {
  position_pelvis_on_saddle(mount);
  attach_feet_to_stirrups(mount);
  calculate_riding_knees(mount);
}

void MountedPoseController::dismount() {

  using HP = HumanProportions;
  m_pose.pelvis_pos = QVector3D(0.0F, HP::WAIST_Y, 0.0F);
  m_pose.foot_l = QVector3D(-0.14F, HP::GROUND_Y + m_pose.foot_y_offset, 0.06F);
  m_pose.foot_r = QVector3D(0.14F, HP::GROUND_Y + m_pose.foot_y_offset, -0.06F);
}

void MountedPoseController::riding_idle(const MountedAttachmentFrame& mount) {
  mount_on_horse(mount);

  auto const sample = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::IdleRest,
  });
  apply_mounted_hand_target(mount, Side::Left, sample.left);
  apply_mounted_hand_target(mount, Side::Right, sample.right);

  update_head_hierarchy(mount, 0.0F, 0.0F, sample.debug_label);
}

void MountedPoseController::riding_leaning(const MountedAttachmentFrame& mount,
                                           float forward_lean,
                                           float side_lean) {
  mount_on_horse(mount);
  apply_lean(mount, forward_lean, side_lean);
}

void MountedPoseController::riding_charging(const MountedAttachmentFrame& mount,
                                            float intensity) {
  mount_on_horse(mount);

  auto const sample = Animation::resolve_mounted_rider_charge_pose({
      .intensity = intensity,
  });
  apply_mounted_posture_sample(m_pose, mount, sample);

  update_head_hierarchy(mount, 0.0F, 0.0F, "riding_charging");

  hold_reins(mount, 0.2F, 0.2F, 0.85F, 0.85F);
}

void MountedPoseController::riding_reining(const MountedAttachmentFrame& mount,
                                           float left_tension,
                                           float right_tension) {
  left_tension = std::clamp(left_tension, 0.0F, 1.0F);
  right_tension = std::clamp(right_tension, 0.0F, 1.0F);

  mount_on_horse(mount);

  QVector3D const left_rein_pos = rein_anchor(mount, Side::Left, 0.15F, left_tension);
  QVector3D const right_rein_pos =
      rein_anchor(mount, Side::Right, 0.15F, right_tension);

  get_hand(Side::Left) = left_rein_pos;
  get_hand(Side::Right) = right_rein_pos;

  const QVector3D left_outward = compute_outward_dir(Side::Left);
  const QVector3D right_outward = compute_outward_dir(Side::Right);
  get_elbow(Side::Left) = solve_elbow_ik(Side::Left,
                                         get_shoulder(Side::Left),
                                         left_rein_pos,
                                         left_outward,
                                         0.52F,
                                         0.08F,
                                         -0.12F,
                                         1.0F);
  get_elbow(Side::Right) = solve_elbow_ik(Side::Right,
                                          get_shoulder(Side::Right),
                                          right_rein_pos,
                                          right_outward,
                                          0.52F,
                                          0.08F,
                                          -0.12F,
                                          1.0F);

  auto const sample = Animation::resolve_mounted_rider_reining_pose({
      .left_tension = left_tension,
      .right_tension = right_tension,
  });
  apply_mounted_posture_sample(m_pose, mount, sample);

  update_head_hierarchy(mount, 0.0F, 0.0F, "riding_reining");
}

void MountedPoseController::riding_melee_strike(const MountedAttachmentFrame& mount,
                                                float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mount_on_horse(mount);
  apply_sword_strike(mount, attack_phase, false);
  enforce_arm_reach(Side::Left);
  enforce_arm_reach(Side::Right);
}

void MountedPoseController::riding_spear_thrust(const MountedAttachmentFrame& mount,
                                                float attack_phase) {
  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  mount_on_horse(mount);
  apply_spear_thrust(mount, attack_phase);
  enforce_arm_reach(Side::Left);
  enforce_arm_reach(Side::Right);
}

void MountedPoseController::riding_bow_shot(const MountedAttachmentFrame& mount,
                                            float draw_phase) {
  mount_on_horse(mount);
  apply_bow_draw(mount, draw_phase);
  enforce_arm_reach(Side::Left);
  enforce_arm_reach(Side::Right);
}

void MountedPoseController::riding_shield_defense(const MountedAttachmentFrame& mount,
                                                  bool raised) {
  mount_on_horse(mount);
  apply_shield_defense(mount, raised);
}

void MountedPoseController::hold_reins(const MountedAttachmentFrame& mount,
                                       float left_slack,
                                       float right_slack,
                                       float left_tension,
                                       float right_tension) {
  mount_on_horse(mount);
  hold_reins_impl(
      mount, left_slack, right_slack, left_tension, right_tension, true, true);
}

void MountedPoseController::hold_spear_mounted(const MountedAttachmentFrame& mount,
                                               SpearGrip grip_style) {
  mount_on_horse(mount);
  apply_spear_guard(mount, grip_style);
}

void MountedPoseController::hold_bow_mounted(const MountedAttachmentFrame& mount) {
  mount_on_horse(mount);
  apply_bow_draw(mount, 0.0F);
}

void MountedPoseController::apply_saddle_clearance(const MountedAttachmentFrame& mount,
                                                   const HorseDimensions& dims,
                                                   float forward_bias,
                                                   float up_bias) {
  float const forward_pull = std::max(0.0F, forward_bias) * (dims.body_width * 0.12F) +
                             dims.seat_forward_offset * 0.30F;
  float const up_lift = std::max(0.0F, up_bias) * (dims.saddle_thickness * 0.85F);

  QVector3D const offset = -mount.seat_forward * forward_pull +
                           mount.seat_up * (up_lift + dims.body_height * 0.01F);
  m_pose.pelvis_pos += offset;
  translate_upper_body(offset);
  calculate_riding_knees(mount);
}

void MountedPoseController::stabilize_upper_body(const MountedAttachmentFrame& mount,
                                                 const HorseDimensions& dims) {
  using HP = HumanProportions;
  QVector3D const world_up(0.0F, 1.0F, 0.0F);

  QVector3D right_flat = mount.seat_right;
  right_flat.setY(0.0F);
  if (right_flat.lengthSquared() < 1e-4F) {
    right_flat = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right_flat.normalize();
  }

  float const height_scale = m_anim_ctx.variation.height_scale;
  QVector3D const shoulder_mid = (m_pose.shoulder_l + m_pose.shoulder_r) * 0.5F;
  QVector3D const desired_mid =
      m_pose.pelvis_pos + world_up * ((HP::SHOULDER_Y - HP::WAIST_Y) * height_scale) -
      mount.seat_forward * (dims.body_length * 0.02F);

  QVector3D const mid_offset = desired_mid - shoulder_mid;
  m_pose.shoulder_l += mid_offset;
  m_pose.shoulder_r += mid_offset;

  float const desired_half = std::clamp(dims.body_width * 0.52F, 0.14F, 0.36F);
  m_pose.shoulder_l = desired_mid - right_flat * desired_half;
  m_pose.shoulder_r = desired_mid + right_flat * desired_half;

  float const target_neck_height = std::max(
      (HP::NECK_BASE_Y - HP::SHOULDER_Y) * height_scale + dims.saddle_thickness * 0.35F,
      m_pose.neck_base.y() - desired_mid.y());
  m_pose.neck_base = desired_mid + world_up * target_neck_height;

  float const head_height =
      std::max((HP::HEAD_CENTER_Y - HP::NECK_BASE_Y) * height_scale,
               m_pose.head_pos.y() - m_pose.neck_base.y());
  m_pose.head_pos = m_pose.neck_base + world_up * head_height;
}

void MountedPoseController::apply_pose(const MountedAttachmentFrame& mount,
                                       const MountedRiderPoseRequest& request) {
  mount_on_horse(mount);
  apply_saddle_clearance(
      mount, request.dims, request.clearance_forward, request.clearance_up);

  stabilize_upper_body(mount, request.dims);

  auto const plan = Animation::resolve_mounted_rider_pose_plan({
      .seat_pose = to_animation_seat_pose(request.seat_pose),
      .weapon_pose = to_animation_weapon_pose(request.weapon_pose),
      .shield_pose = to_animation_shield_pose(request.shield_pose),
      .forward_bias = request.forward_bias,
      .left_hand_on_reins = request.left_hand_on_reins,
      .right_hand_on_reins = request.right_hand_on_reins,
  });
  float const forward = plan.forward_bias;
  apply_lean(mount, forward, request.side_bias);

  apply_torso_sculpt(
      mount, request.torso_compression, request.torso_twist, request.shoulder_dip);

  float const clamped_forward = std::clamp(forward, -1.0F, 1.0F);
  float const clamped_side = std::clamp(request.side_bias, -1.0F, 1.0F);
  update_head_hierarchy(
      mount, clamped_forward * 0.4F, clamped_side * 0.4F, "applyPose_fixup");

  if (plan.apply_left_rein || plan.apply_right_rein) {
    hold_reins_impl(mount,
                    request.rein_slack_left,
                    request.rein_slack_right,
                    request.rein_tension_left,
                    request.rein_tension_right,
                    plan.apply_left_rein,
                    plan.apply_right_rein);
  }

  switch (request.shield_pose) {
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

  switch (request.weapon_pose) {
  case MountedWeaponPose::SwordIdle:
    apply_sword_idle_pose(mount, request.dims);
    break;
  case MountedWeaponPose::SwordStrike:
    apply_sword_strike(mount, request.action_phase, plan.sword_strike_keeps_left_hand);
    break;
  case MountedWeaponPose::SpearGuard:
    apply_spear_guard(mount, SpearGrip::OVERHAND);
    break;
  case MountedWeaponPose::SpearThrust:
    apply_spear_thrust(mount, request.action_phase);
    break;
  case MountedWeaponPose::BowDraw:
    apply_bow_draw(mount, request.action_phase);
    break;
  case MountedWeaponPose::None:
  default:
    break;
  }

  enforce_arm_reach(Side::Left);
  enforce_arm_reach(Side::Right);
}

void MountedPoseController::apply_lean(const MountedAttachmentFrame& mount,
                                       float forward_lean,
                                       float side_lean) {
  auto const sample = Animation::resolve_mounted_rider_lean_pose({
      .forward_lean = forward_lean,
      .side_lean = side_lean,
  });
  apply_mounted_posture_sample(m_pose, mount, sample);

  update_head_hierarchy(
      mount, sample.head_forward_tilt, sample.head_side_tilt, "apply_lean");
}

void MountedPoseController::apply_shield_defense(const MountedAttachmentFrame& mount,
                                                 bool raised) {
  auto const sample = Animation::resolve_mounted_shield_defense_pose({raised});
  QVector3D const shield_pos = seat_relative(mount, sample.left_hand);
  QVector3D const rein_pos = rein_anchor(
      mount, Side::Right, sample.right_rein_slack, sample.right_rein_tension);

  get_hand(Side::Left) = shield_pos;
  get_hand(Side::Right) = rein_pos;

  const QVector3D left_outward = compute_outward_dir(Side::Left);
  const QVector3D right_outward = compute_outward_dir(Side::Right);
  get_elbow(Side::Left) = solve_elbow_ik(Side::Left,
                                         get_shoulder(Side::Left),
                                         shield_pos,
                                         left_outward,
                                         0.45F,
                                         0.15F,
                                         -0.10F,
                                         1.0F);
  get_elbow(Side::Right) = solve_elbow_ik(Side::Right,
                                          get_shoulder(Side::Right),
                                          rein_pos,
                                          right_outward,
                                          0.45F,
                                          0.12F,
                                          -0.08F,
                                          1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, sample.debug_label);
}

void MountedPoseController::apply_shield_stowed(const MountedAttachmentFrame& mount,
                                                const HorseDimensions& dims) {
  auto const sample = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::ShieldStowed,
      .body_length = dims.body_length,
      .body_width = dims.body_width,
      .saddle_thickness = dims.saddle_thickness,
  });
  apply_mounted_hand_target(mount, Side::Left, sample.left);

  update_head_hierarchy(mount, 0.0F, 0.0F, sample.debug_label);
}

void MountedPoseController::apply_sword_idle_pose(const MountedAttachmentFrame& mount,
                                                  const HorseDimensions& dims) {
  auto const sample = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::SwordIdle,
      .body_length = dims.body_length,
      .body_width = dims.body_width,
      .body_height = dims.body_height,
      .saddle_thickness = dims.saddle_thickness,
  });
  apply_mounted_hand_target(mount, Side::Right, sample.right);

  update_head_hierarchy(mount, 0.0F, 0.0F, sample.debug_label);
}

void MountedPoseController::apply_sword_strike(const MountedAttachmentFrame& mount,
                                               float attack_phase,
                                               bool keep_left_hand) {
  auto const sample = Animation::resolve_mounted_sword_strike_pose({attack_phase});

  QVector3D const hand_r_target = seat_relative(mount, sample.right_hand);
  QVector3D const hand_l_target =
      rein_anchor(mount, Side::Left, sample.left_rein_slack, sample.left_rein_tension) +
      mount.seat_up * sample.left_hand_up_offset;

  if (std::abs(sample.torso_twist) > 0.001F) {
    QVector3D const twist_offset = mount.seat_forward * sample.torso_twist;
    m_pose.shoulder_r += twist_offset;
    m_pose.shoulder_l -= twist_offset * 0.5F;
  }

  if (sample.side_lean > 0.001F) {
    QVector3D const lean_offset = mount.seat_right * sample.side_lean;
    m_pose.shoulder_l += lean_offset;
    m_pose.shoulder_r += lean_offset;
    m_pose.neck_base += lean_offset * 0.8F;
  }

  if (sample.forward_lean > 0.001F) {
    QVector3D const lean_offset = mount.seat_forward * sample.forward_lean;
    m_pose.shoulder_l += lean_offset;
    m_pose.shoulder_r += lean_offset;
    m_pose.neck_base += lean_offset * 0.9F;
  }

  if (std::abs(sample.torso_commit) > 0.001F) {
    QVector3D const commit_offset = mount.seat_forward * sample.torso_commit;
    m_pose.shoulder_r += commit_offset;
    m_pose.shoulder_l += commit_offset * 0.45F;
    m_pose.neck_base += commit_offset * 0.65F;
  }

  if (sample.counter_lift > 0.001F) {
    QVector3D const lift_offset = mount.seat_up * sample.counter_lift;
    m_pose.shoulder_l += lift_offset * 0.70F;
    m_pose.neck_base += lift_offset * 0.45F;
  }

  if (std::abs(sample.shoulder_dip) > 0.001F) {
    m_pose.shoulder_r += mount.seat_up * sample.shoulder_dip;
  }

  update_head_hierarchy(
      mount, sample.head_forward_tilt, sample.head_side_tilt, sample.debug_label);

  get_hand(Side::Right) = hand_r_target;
  if (!keep_left_hand) {
    get_hand(Side::Left) = hand_l_target;
  }

  const QVector3D right_outward = compute_outward_dir(Side::Right);
  get_elbow(Side::Right) = solve_elbow_ik(Side::Right,
                                          get_shoulder(Side::Right),
                                          hand_r_target,
                                          right_outward,
                                          0.42F,
                                          0.15F,
                                          0.0F,
                                          1.0F);

  if (!keep_left_hand) {
    const QVector3D left_outward = compute_outward_dir(Side::Left);
    get_elbow(Side::Left) = solve_elbow_ik(Side::Left,
                                           get_shoulder(Side::Left),
                                           hand_l_target,
                                           left_outward,
                                           0.45F,
                                           0.12F,
                                           -0.08F,
                                           1.0F);
  }
}

void MountedPoseController::apply_spear_thrust(const MountedAttachmentFrame& mount,
                                               float attack_phase) {
  auto const sample = Animation::resolve_mounted_spear_thrust_pose({attack_phase});

  QVector3D const hand_r_target = seat_relative(mount, sample.right_hand);
  QVector3D const hand_l_target = seat_relative(mount, sample.left_hand);

  update_head_hierarchy(mount, sample.head_forward_tilt, 0.0F, sample.debug_label);

  if (sample.forward_lean > 0.001F) {
    QVector3D const lean_offset = mount.seat_forward * sample.forward_lean;
    m_pose.shoulder_l += lean_offset;
    m_pose.shoulder_r += lean_offset;
    m_pose.neck_base += lean_offset * 0.85F;
  }

  if (std::abs(sample.torso_twist) > 0.001F) {
    QVector3D const twist_offset = mount.seat_forward * sample.torso_twist;
    m_pose.shoulder_r += twist_offset;
    m_pose.shoulder_l -= twist_offset * 0.3F;
  }

  if (sample.shoulder_drop > 0.001F) {
    m_pose.shoulder_r -= mount.seat_up * sample.shoulder_drop;
    m_pose.shoulder_l -= mount.seat_up * (sample.shoulder_drop * 0.3F);
  }

  if (sample.torso_compression > 0.001F) {
    m_pose.shoulder_l -= mount.seat_up * sample.torso_compression;
    m_pose.shoulder_r -= mount.seat_up * sample.torso_compression;
    m_pose.neck_base -= mount.seat_up * (sample.torso_compression * 0.6F);
  }

  get_hand(Side::Right) = hand_r_target;
  get_hand(Side::Left) = hand_l_target;

  const QVector3D left_outward = compute_outward_dir(Side::Left);
  const QVector3D right_outward = compute_outward_dir(Side::Right);
  get_elbow(Side::Left) = solve_elbow_ik(Side::Left,
                                         get_shoulder(Side::Left),
                                         hand_l_target,
                                         left_outward,
                                         0.48F,
                                         0.10F,
                                         -0.06F,
                                         1.0F);
  get_elbow(Side::Right) = solve_elbow_ik(Side::Right,
                                          get_shoulder(Side::Right),
                                          hand_r_target,
                                          right_outward,
                                          0.48F,
                                          0.10F,
                                          -0.04F,
                                          1.0F);
}

void MountedPoseController::apply_spear_guard(const MountedAttachmentFrame& mount,
                                              SpearGrip grip_style) {
  auto const sample = Animation::resolve_mounted_spear_guard_pose({
      .grip = to_animation_spear_grip(grip_style),
  });
  QVector3D const hand_r_target = seat_relative(mount, sample.right_hand);
  QVector3D const hand_l_target =
      sample.left_hand_uses_rein
          ? rein_anchor(
                mount, Side::Left, sample.left_rein_slack, sample.left_rein_tension)
          : seat_relative(mount, sample.left_hand);

  get_hand(Side::Right) = hand_r_target;
  get_hand(Side::Left) = hand_l_target;

  const QVector3D left_outward = compute_outward_dir(Side::Left);
  const QVector3D right_outward = compute_outward_dir(Side::Right);
  get_elbow(Side::Left) = solve_elbow_ik(Side::Left,
                                         get_shoulder(Side::Left),
                                         hand_l_target,
                                         left_outward,
                                         0.45F,
                                         0.12F,
                                         -0.08F,
                                         1.0F);
  get_elbow(Side::Right) = solve_elbow_ik(Side::Right,
                                          get_shoulder(Side::Right),
                                          hand_r_target,
                                          right_outward,
                                          0.45F,
                                          0.12F,
                                          -0.05F,
                                          1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, sample.debug_label);
}

void MountedPoseController::apply_bow_draw(const MountedAttachmentFrame& mount,
                                           float draw_phase) {
  auto const sample = Animation::resolve_mounted_bow_draw_pose({draw_phase});
  QVector3D const hand_l_target = seat_relative(mount, sample.left_hand);
  QVector3D const hand_r_target =
      seat_relative(mount, sample.right_hand) +
      QVector3D(0.0F, sample.right_hand_world_y_offset, 0.0F);

  get_hand(Side::Left) = hand_l_target;
  get_hand(Side::Right) = hand_r_target;

  const QVector3D left_outward = compute_outward_dir(Side::Left);
  const QVector3D right_outward = compute_outward_dir(Side::Right);
  get_elbow(Side::Left) = solve_elbow_ik(Side::Left,
                                         get_shoulder(Side::Left),
                                         hand_l_target,
                                         left_outward,
                                         0.50F,
                                         0.08F,
                                         -0.05F,
                                         1.0F);
  get_elbow(Side::Right) = solve_elbow_ik(Side::Right,
                                          get_shoulder(Side::Right),
                                          hand_r_target,
                                          right_outward,
                                          0.48F,
                                          0.12F,
                                          -0.08F,
                                          1.0F);

  update_head_hierarchy(mount, 0.0F, 0.0F, sample.debug_label);
}

void MountedPoseController::apply_torso_sculpt(const MountedAttachmentFrame& mount,
                                               float compression,
                                               float twist,
                                               float shoulder_dip) {
  auto const sample = Animation::resolve_mounted_rider_torso_sculpt_pose({
      .compression = compression,
      .twist = twist,
      .shoulder_dip = shoulder_dip,
  });
  apply_mounted_posture_sample(m_pose, mount, sample);
}

void MountedPoseController::hold_reins_impl(const MountedAttachmentFrame& mount,
                                            float left_slack,
                                            float right_slack,
                                            float left_tension,
                                            float right_tension,
                                            bool apply_left,
                                            bool apply_right) {
  auto const sample = Animation::resolve_mounted_rider_hand_pose({
      .kind = Animation::MountedRiderHandPoseKind::ReinHold,
      .left_rein_slack = left_slack,
      .right_rein_slack = right_slack,
      .left_rein_tension = left_tension,
      .right_rein_tension = right_tension,
  });

  if (apply_left) {
    apply_mounted_hand_target(mount, Side::Left, sample.left);
    enforce_arm_reach(Side::Left);
  }

  if (apply_right) {
    apply_mounted_hand_target(mount, Side::Right, sample.right);
    enforce_arm_reach(Side::Right);
  }
}

void MountedPoseController::apply_mounted_hand_target(
    const MountedAttachmentFrame& mount,
    Side side,
    const Animation::MountedHandPoseTarget& target) {
  if (!target.active) {
    return;
  }

  QVector3D const hand =
      target.uses_rein
          ? rein_anchor(mount, side, target.rein_slack, target.rein_tension)
          : mounted_hand_anchor(m_pose, mount, target.anchor) +
                mounted_offset(mount, target.offset);

  get_hand(side) = hand;
  QVector3D const outward = compute_outward_dir(side);
  get_elbow(side) = solve_elbow_ik(side,
                                   get_shoulder(side),
                                   hand,
                                   outward,
                                   target.elbow.along_frac,
                                   target.elbow.lateral_offset,
                                   target.elbow.y_bias,
                                   target.elbow.outward_sign);
}

void MountedPoseController::attach_feet_to_stirrups(
    const MountedAttachmentFrame& mount) {
  QVector3D seat_right = mount.seat_right;
  if (seat_right.lengthSquared() < 1e-6F) {
    seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    seat_right.normalize();
  }

  float const height_scale = m_anim_ctx.variation.height_scale;
  float const stirrup_half_spread =
      0.5F * std::abs(QVector3D::dotProduct(
                 mount.stirrup_bottom_right - mount.stirrup_bottom_left, seat_right));
  float const foot_outboard_bias =
      std::max(0.03F * height_scale, stirrup_half_spread * 0.16F);

  m_pose.foot_l = mount.stirrup_bottom_left - seat_right * foot_outboard_bias;
  m_pose.foot_r = mount.stirrup_bottom_right + seat_right * foot_outboard_bias;
}

void MountedPoseController::position_pelvis_on_saddle(
    const MountedAttachmentFrame& mount) {
  using HP = HumanProportions;
  QVector3D const seat_world = mount.seat_position;
  QVector3D const delta = seat_world - m_pose.pelvis_pos;
  m_pose.pelvis_pos = seat_world;
  translate_upper_body(delta);

  float const height_scale = m_anim_ctx.variation.height_scale;
  float const target_shoulder_height = (HP::SHOULDER_Y - HP::WAIST_Y) * height_scale;
  QVector3D const shoulder_mid = (m_pose.shoulder_l + m_pose.shoulder_r) * 0.5F;
  float const current_shoulder_height = shoulder_mid.y() - m_pose.pelvis_pos.y();
  if (current_shoulder_height < target_shoulder_height) {
    translate_upper_body(mount.seat_up *
                         (target_shoulder_height - current_shoulder_height));
  }
}

void MountedPoseController::translate_upper_body(const QVector3D& delta) {
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
    const MountedAttachmentFrame& mount) {
  using HP = HumanProportions;
  QVector3D seat_right = mount.seat_right;
  if (seat_right.lengthSquared() < 1e-6F) {
    seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    seat_right.normalize();
  }
  QVector3D seat_forward = mount.seat_forward;
  if (seat_forward.lengthSquared() < 1e-6F) {
    seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    seat_forward.normalize();
  }

  float const height_scale = m_anim_ctx.variation.height_scale;
  float const stirrup_half_spread =
      0.5F * std::abs(QVector3D::dotProduct(
                 mount.stirrup_bottom_right - mount.stirrup_bottom_left, seat_right));
  float const hip_spread = std::max({HP::HIP_LATERAL_OFFSET * height_scale * 2.35F,
                                     0.18F * height_scale,
                                     stirrup_half_spread * 0.82F});
  QVector3D const hip_offset =
      mount.seat_up * (-0.06F * height_scale) - seat_forward * (-0.02F * height_scale);
  QVector3D const hip_left = m_pose.pelvis_pos - seat_right * hip_spread + hip_offset;
  QVector3D const hip_right = m_pose.pelvis_pos + seat_right * hip_spread + hip_offset;

  auto solve_mounted_knee =
      [&](Side side, const QVector3D& hip, const QVector3D& foot) {
        QVector3D knee = solve_knee_ik(side, hip, foot, height_scale);
        QVector3D const outward = (side == Side::Left) ? -seat_right : seat_right;
        float const foot_out = QVector3D::dotProduct(foot - m_pose.pelvis_pos, outward);
        float const minimum_knee_out =
            std::max(hip_spread + 0.02F * height_scale, stirrup_half_spread * 0.88F);
        float const target_knee_out = std::clamp(foot_out - 0.005F * height_scale,
                                                 minimum_knee_out,
                                                 foot_out + 0.03F * height_scale);
        float const knee_out = QVector3D::dotProduct(knee - m_pose.pelvis_pos, outward);
        if (std::abs(knee_out - target_knee_out) > 1.0e-4F) {
          knee += outward * (target_knee_out - knee_out);
        }

        float const foot_forward = QVector3D::dotProduct(foot - hip, seat_forward);
        float const knee_forward = QVector3D::dotProduct(knee - hip, seat_forward);
        float const minimum_knee_forward = foot_forward + 0.10F * height_scale;
        if (knee_forward < minimum_knee_forward) {
          knee += seat_forward * (minimum_knee_forward - knee_forward);
        }

        float const knee_floor = HP::GROUND_Y + m_pose.foot_y_offset * 0.5F;
        if (knee.y() < knee_floor) {
          knee.setY(knee_floor);
        }
        if (knee.y() > hip.y()) {
          knee.setY(hip.y());
        }
        return knee;
      };

  m_pose.knee_l = solve_mounted_knee(Side::Left, hip_left, m_pose.foot_l);
  m_pose.knee_r = solve_mounted_knee(Side::Right, hip_right, m_pose.foot_r);
}

auto MountedPoseController::solve_elbow_ik(Side,
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

auto MountedPoseController::solve_knee_ik(Side side,
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
       .bend_preference = (side == Side::Left) ? QVector3D(-0.70F, -0.15F, 0.30F)
                                               : QVector3D(0.70F, -0.15F, 0.30F),
       .clamp_to_hip_height = true});
}

auto MountedPoseController::get_shoulder(Side side) const -> const QVector3D& {
  return (side == Side::Left) ? m_pose.shoulder_l : m_pose.shoulder_r;
}

auto MountedPoseController::get_hand(Side side) -> QVector3D& {
  return (side == Side::Left) ? m_pose.hand_l : m_pose.hand_r;
}

auto MountedPoseController::get_hand(Side side) const -> const QVector3D& {
  return (side == Side::Left) ? m_pose.hand_l : m_pose.hand_r;
}

auto MountedPoseController::get_elbow(Side side) -> QVector3D& {
  return (side == Side::Left) ? m_pose.elbow_l : m_pose.elbow_r;
}

auto MountedPoseController::compute_right_axis() const -> QVector3D {
  return Render::Humanoid::PosePrimitives::compute_right_axis(m_pose);
}

auto MountedPoseController::compute_outward_dir(Side side) const -> QVector3D {
  return Render::Humanoid::PosePrimitives::compute_outward_dir(m_pose, side);
}

void MountedPoseController::enforce_arm_reach(Side side) {
  QVector3D const shoulder = get_shoulder(side);
  QVector3D shoulder_to_hand = get_hand(side) - shoulder;
  constexpr float k_max_arm_reach =
      (HumanProportions::UPPER_ARM_LEN + HumanProportions::FORE_ARM_LEN) * 0.75F;
  float const requested_reach = shoulder_to_hand.length();
  if (requested_reach > k_max_arm_reach && requested_reach > 1.0e-6F) {
    shoulder_to_hand *= k_max_arm_reach / requested_reach;
    get_hand(side) = shoulder + shoulder_to_hand;
  }

  get_elbow(side) = solve_elbow_ik(side,
                                   shoulder,
                                   get_hand(side),
                                   compute_outward_dir(side),
                                   0.48F,
                                   0.10F,
                                   -0.05F,
                                   1.0F);
}

void MountedPoseController::apply_fixed_head_frame(const MountedAttachmentFrame& mount,
                                                   float extra_forward_tilt,
                                                   float extra_side_tilt,
                                                   std::string_view debug_label) {
  using HP = HumanProportions;
  (void)debug_label;
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

  float const forward_tilt = std::clamp(extra_forward_tilt, -0.35F, 0.45F);
  float const side_tilt = std::clamp(extra_side_tilt, -0.25F, 0.25F);
  QVector3D tilted_up = up_dir + fwd_dir * forward_tilt + right_dir * side_tilt;
  if (tilted_up.lengthSquared() < 1e-6F) {
    tilted_up = up_dir;
  } else {
    tilted_up.normalize();
  }

  QVector3D tilted_forward = mount.seat_forward;
  if (tilted_forward.lengthSquared() < 1e-6F) {
    tilted_forward = fwd_dir;
  } else {
    tilted_forward.normalize();
  }
  tilted_forward -= tilted_up * QVector3D::dotProduct(tilted_forward, tilted_up);
  if (tilted_forward.lengthSquared() < 1e-6F) {
    tilted_forward = QVector3D::crossProduct(tilted_up, right_dir);
  }
  if (tilted_forward.lengthSquared() < 1e-6F) {
    tilted_forward = fwd_dir;
  } else {
    tilted_forward.normalize();
  }

  right_dir = QVector3D::crossProduct(tilted_forward, tilted_up);
  if (right_dir.lengthSquared() < 1e-6F) {
    right_dir = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right_dir.normalize();
  }
  tilted_forward = QVector3D::crossProduct(tilted_up, right_dir).normalized();

  m_pose.head_pos = m_pose.neck_base + tilted_up * neck_len;

  m_pose.head_frame.origin = m_pose.head_pos;
  m_pose.head_frame.up = tilted_up;
  m_pose.head_frame.right = right_dir;
  m_pose.head_frame.forward = tilted_forward;
  if (m_pose.head_r < 0.01F) {
    m_pose.head_r = 0.12F;
  }
  m_pose.head_frame.radius = m_pose.head_r;
  m_pose.body_frames.head = m_pose.head_frame;
}

void MountedPoseController::update_head_hierarchy(const MountedAttachmentFrame& mount,
                                                  float extra_forward_tilt,
                                                  float extra_side_tilt,
                                                  std::string_view debug_label) {
  apply_fixed_head_frame(mount, extra_forward_tilt, extra_side_tilt, debug_label);
}

void MountedPoseController::finalize_head_sync(const MountedAttachmentFrame& mount,
                                               std::string_view debug_label) {
  apply_fixed_head_frame(mount, 0.0F, 0.0F, debug_label);
}

} // namespace Render::GL
