#include "render/creature/humanoid/runtime/mounted_pose_controller_internal.h"

namespace Render::GL {

using namespace mounted_pose_controller_detail;

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
  aim_mounted_spear(m_pose, mount, sample.spear_direction);

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
  aim_mounted_spear(m_pose, mount, sample.spear_direction);

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
  float const k_max_arm_reach =
      Render::Humanoid::PosePrimitives::humanoid_arm_reach_limit(
          1.0F, Render::Humanoid::PosePrimitives::k_seated_arm_reach_fraction);
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
