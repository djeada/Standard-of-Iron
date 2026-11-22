#include <algorithm>

#include "render/creature/humanoid/runtime/mounted_pose_controller_internal.h"

namespace Render::GL {

namespace mounted_pose_controller_detail {

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

void aim_mounted_spear(HumanoidPose& pose,
                       const MountedAttachmentFrame& mount,
                       Animation::MountedSeatOffset direction) {
  QVector3D const wanted = mount.seat_forward * direction.forward +
                           mount.seat_right * direction.right +
                           mount.seat_up * direction.up;
  if (wanted.lengthSquared() < 1.0e-6F) {
    return;
  }
  pose.grip_axis_r = Render::Humanoid::hand_axis_for_weapon_direction(
      wanted.normalized(), resolve_spear_direction(AnimationInputs{}), true);
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
  case SpearGrip::UPRIGHT:
    return Animation::MountedSpearGuardGrip::Upright;
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

} // namespace mounted_pose_controller_detail

using namespace mounted_pose_controller_detail;

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

void MountedPoseController::grip_spear(const MountedAttachmentFrame& mount,
                                       SpearGrip grip_style) {
  apply_spear_guard(mount, grip_style);
  enforce_arm_reach(Side::Left);
  enforce_arm_reach(Side::Right);
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

} // namespace Render::GL
