#include "humanoid_full_builder.h"

#include "../gl/humanoid/humanoid_constants.h"
#include "humanoid_specs.h"
#include "skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::Humanoid {

namespace {

namespace Creature = Render::Creature;
using HP = Render::GL::HumanProportions;

constexpr auto bi(HumanoidBone b) noexcept -> Creature::BoneIndex {
  return static_cast<Creature::BoneIndex>(b);
}

// Frame-local position used by the eye lookups — mirrors
// `frame_local_position` in render/geom/math_utils.h but is duplicated
// here to avoid pulling extra headers into this compilation unit.
auto frame_local(const Render::GL::AttachmentFrame &f,
                 const QVector3D &local) noexcept -> QVector3D {
  return f.origin + f.right * local.x() * f.radius +
         f.up * local.y() * f.radius + f.forward * local.z() * f.radius;
}

} // namespace

void compute_humanoid_body_metrics(const Render::GL::HumanoidPose &pose,
                                   const QVector3D &proportion_scaling,
                                   float torso_scale,
                                   HumanoidBodyMetrics &out) noexcept {
  float const width_scale = proportion_scaling.x();
  float const depth_scale = proportion_scaling.z();

  QVector3D right = pose.shoulder_r - pose.shoulder_l;
  right.setY(0.0F);
  right.setZ(0.0F);
  if (right.lengthSquared() < 1e-8F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right.normalize();
  if (right.x() < 0.0F) {
    right = -right;
  }
  out.right_axis = right;
  out.up_axis = QVector3D(0.0F, 1.0F, 0.0F);

  QVector3D forward = QVector3D::crossProduct(right, out.up_axis);
  if (forward.lengthSquared() < 1e-8F) {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  forward.normalize();
  out.forward_axis = forward;

  QVector3D const shoulder_mid = (pose.shoulder_l + pose.shoulder_r) * 0.5F;
  float const shoulder_half_span =
      0.5F * std::abs(pose.shoulder_r.x() - pose.shoulder_l.x());
  out.shoulder_half_span = shoulder_half_span;

  float const torso_r_base =
      std::max(HP::TORSO_TOP_R, shoulder_half_span * 0.95F);
  out.torso_r = torso_r_base * torso_scale;

  float const torso_depth_factor =
      std::clamp(HP::TORSO_DEPTH_FACTOR_BASE + (depth_scale - 1.0F) * 0.20F,
                 HP::TORSO_DEPTH_FACTOR_MIN, HP::TORSO_DEPTH_FACTOR_MAX);
  out.torso_depth = out.torso_r * torso_depth_factor;

  float const y_shoulder = shoulder_mid.y();
  float const y_neck = pose.neck_base.y();
  out.y_top_cover =
      std::max(y_shoulder + 0.00F, y_neck + HP::TORSO_TOP_COVER_OFFSET);

  out.upper_arm_r = HP::UPPER_ARM_R * width_scale;
  out.fore_arm_r = HP::FORE_ARM_R * width_scale;
  out.joint_r = HP::HAND_RADIUS * width_scale * 1.05F;
  out.hand_r = HP::HAND_RADIUS * width_scale * 0.95F;

  out.leg_joint_r = HP::LOWER_LEG_R * width_scale * 0.95F;
  out.thigh_r = HP::UPPER_LEG_R * width_scale;
  out.shin_r = HP::LOWER_LEG_R * width_scale;
  out.foot_radius = out.shin_r * 1.10F;
}

void compute_humanoid_body_frames(
    Render::GL::HumanoidPose &pose,
    const HumanoidBodyMetrics &m) noexcept {
  using AF = Render::GL::AttachmentFrame;

  QVector3D const shoulder_mid = (pose.shoulder_l + pose.shoulder_r) * 0.5F;
  float const y_shoulder = shoulder_mid.y();

  // Head frame: already populated by draw_common_body's head RigDSL block
  // as `pose.head_frame`. Mirror it to body_frames.head; the body_frames
  // write is what downstream equipment reads.
  pose.body_frames.head = pose.head_frame;

  QVector3D const torso_center = QVector3D(
      (shoulder_mid.x() + pose.pelvis_pos.x()) * 0.5F, y_shoulder, 0.0F);
  pose.body_frames.torso.origin = torso_center;
  pose.body_frames.torso.right = m.right_axis;
  pose.body_frames.torso.up = m.up_axis;
  pose.body_frames.torso.forward = m.forward_axis;
  pose.body_frames.torso.radius = m.torso_r;
  pose.body_frames.torso.depth = m.torso_depth;

  pose.body_frames.back.origin =
      torso_center - m.forward_axis * m.torso_depth;
  pose.body_frames.back.right = m.right_axis;
  pose.body_frames.back.up = m.up_axis;
  pose.body_frames.back.forward = -m.forward_axis;
  pose.body_frames.back.radius = m.torso_r * 0.75F;
  pose.body_frames.back.depth = m.torso_depth * 0.90F;

  pose.body_frames.waist.origin = pose.pelvis_pos;
  pose.body_frames.waist.right = m.right_axis;
  pose.body_frames.waist.up = m.up_axis;
  pose.body_frames.waist.forward = m.forward_axis;
  pose.body_frames.waist.radius = m.torso_r * 0.80F;
  pose.body_frames.waist.depth = m.torso_depth * 0.72F;

  // Shoulders — locally-flipped right axis so "outward" along the arm is
  // consistent regardless of side.
  QVector3D shoulder_up = (pose.shoulder_l - pose.pelvis_pos).normalized();
  QVector3D shoulder_fwd_l =
      QVector3D::crossProduct(-m.right_axis, shoulder_up);
  if (shoulder_fwd_l.lengthSquared() < 1e-8F) {
    shoulder_fwd_l = m.forward_axis;
  } else {
    shoulder_fwd_l.normalize();
  }
  pose.body_frames.shoulder_l = AF{pose.shoulder_l, -m.right_axis,
                                   shoulder_up, shoulder_fwd_l,
                                   m.upper_arm_r, 0.0F};

  QVector3D shoulder_fwd_r =
      QVector3D::crossProduct(m.right_axis, shoulder_up);
  if (shoulder_fwd_r.lengthSquared() < 1e-8F) {
    shoulder_fwd_r = m.forward_axis;
  } else {
    shoulder_fwd_r.normalize();
  }
  pose.body_frames.shoulder_r = AF{pose.shoulder_r, m.right_axis,
                                   shoulder_up, shoulder_fwd_r,
                                   m.upper_arm_r, 0.0F};

  auto compute_hand = [&](const QVector3D &elbow, const QVector3D &hand,
                          float right_sign) -> AF {
    QVector3D up = hand - elbow;
    if (up.lengthSquared() > 1e-8F) {
      up.normalize();
    } else {
      up = m.up_axis;
    }
    QVector3D fwd =
        QVector3D::crossProduct(m.right_axis * right_sign, up);
    if (fwd.lengthSquared() < 1e-8F) {
      fwd = m.forward_axis;
    } else {
      fwd.normalize();
    }
    return AF{hand, m.right_axis * right_sign, up, fwd, m.hand_r, 0.0F};
  };
  pose.body_frames.hand_l = compute_hand(pose.elbow_l, pose.hand_l, -1.0F);
  pose.body_frames.hand_r = compute_hand(pose.elbow_r, pose.hand_r, 1.0F);

  auto compute_foot_fwd = [&](float lateral_sign) -> QVector3D {
    QVector3D f = m.forward_axis + m.right_axis * (0.12F * lateral_sign);
    if (f.lengthSquared() > 1e-8F) {
      f.normalize();
    } else {
      f = m.forward_axis;
    }
    return f;
  };
  pose.body_frames.foot_l = AF{pose.foot_l, -m.right_axis, m.up_axis,
                               compute_foot_fwd(-1.0F), m.foot_radius, 0.0F};
  pose.body_frames.foot_r = AF{pose.foot_r, m.right_axis, m.up_axis,
                               compute_foot_fwd(+1.0F), m.foot_radius, 0.0F};

  auto compute_shin = [&](const QVector3D &ankle, const QVector3D &knee,
                          float right_sign) -> AF {
    AF shin{};
    shin.origin = ankle;
    QVector3D shin_dir = knee - ankle;
    float shin_len = shin_dir.length();
    shin.up = (shin_len > 1e-6F) ? shin_dir / shin_len : m.up_axis;
    QVector3D shin_fwd = m.forward_axis;
    shin_fwd = shin_fwd - shin.up * QVector3D::dotProduct(shin_fwd, shin.up);
    if (shin_fwd.lengthSquared() > 1e-6F) {
      shin_fwd.normalize();
    } else {
      shin_fwd = m.forward_axis;
    }
    shin.forward = shin_fwd;
    shin.right = QVector3D::crossProduct(shin.up, shin.forward) * right_sign;
    shin.radius = HP::LOWER_LEG_R;
    return shin;
  };
  pose.body_frames.shin_l = compute_shin(pose.foot_l, pose.knee_l, -1.0F);
  pose.body_frames.shin_r = compute_shin(pose.foot_r, pose.knee_r, 1.0F);
}

void compute_humanoid_head_frame(Render::GL::HumanoidPose &pose,
                                 const HumanoidBodyMetrics &m) noexcept {
  // Preserve any upstream override (e.g. look-target animation has
  // already filled head_frame with a final orientation).
  if (pose.head_frame.radius > 0.001F) {
    pose.head_frame.origin = pose.head_pos;
    return;
  }

  QVector3D head_up = pose.head_pos - pose.neck_base;
  if (head_up.lengthSquared() < 1e-8F) {
    head_up = m.up_axis;
  } else {
    head_up.normalize();
  }

  QVector3D head_right =
      m.right_axis - head_up * QVector3D::dotProduct(m.right_axis, head_up);
  if (head_right.lengthSquared() < 1e-8F) {
    head_right = QVector3D::crossProduct(head_up, m.forward_axis);
    if (head_right.lengthSquared() < 1e-8F) {
      head_right = QVector3D(1.0F, 0.0F, 0.0F);
    }
  }
  head_right.normalize();

  if (QVector3D::dotProduct(head_right, m.right_axis) < 0.0F) {
    head_right = -head_right;
  }

  QVector3D head_forward = QVector3D::crossProduct(head_right, head_up);
  if (head_forward.lengthSquared() < 1e-8F) {
    head_forward = m.forward_axis;
  } else {
    head_forward.normalize();
  }

  if (QVector3D::dotProduct(head_forward, m.forward_axis) < 0.0F) {
    head_right = -head_right;
    head_forward = -head_forward;
  }

  pose.head_frame.origin = pose.head_pos;
  pose.head_frame.right = head_right;
  pose.head_frame.up = head_up;
  pose.head_frame.forward = head_forward;
  pose.head_frame.radius = pose.head_r;
}

} // namespace Render::Humanoid
