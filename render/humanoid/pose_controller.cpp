#include "pose_controller.h"
#include "humanoid_math.h"
#include "spear_pose_utils.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

HumanoidPoseController::HumanoidPoseController(
    HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx)
    : m_pose(pose), m_anim_ctx(anim_ctx) {}

void HumanoidPoseController::standIdle() {}

void HumanoidPoseController::kneel(float depth) {
  using HP = HumanProportions;

  depth = std::clamp(depth, 0.0F, 1.0F);

  if (depth < 1e-6F) {
    return;
  }

  float const kneel_offset = depth * 0.40F;
  float const pelvis_y = HP::WAIST_Y - kneel_offset;
  m_pose.pelvis_pos.setY(pelvis_y);

  float const stance_narrow = 0.11F;

  float const left_knee_y = HP::GROUND_Y + 0.07F * depth;
  float const left_knee_z = -0.06F * depth;
  m_pose.knee_l = QVector3D(-stance_narrow, left_knee_y, left_knee_z);
  m_pose.foot_l = QVector3D(-stance_narrow - 0.025F, HP::GROUND_Y,
                            left_knee_z - HP::LOWER_LEG_LEN * 0.93F * depth);

  float const right_knee_y = pelvis_y - 0.12F;
  float const right_foot_z = 0.28F * depth;
  m_pose.knee_r = QVector3D(stance_narrow, right_knee_y, right_foot_z - 0.05F);
  m_pose.foot_r = QVector3D(stance_narrow, HP::GROUND_Y + m_pose.foot_y_offset,
                            right_foot_z);

  float const upper_body_drop = kneel_offset;
  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - upper_body_drop);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - upper_body_drop);
  m_pose.neck_base.setY(m_pose.neck_base.y() - upper_body_drop);
  m_pose.head_pos.setY(m_pose.head_pos.y() - upper_body_drop);
}

void HumanoidPoseController::lean(const QVector3D &direction, float amount) {

  amount = std::clamp(amount, 0.0F, 1.0F);

  QVector3D dir = direction;
  if (dir.lengthSquared() > 1e-6F) {
    dir.normalize();
  } else {
    dir = QVector3D(0.0F, 0.0F, 1.0F);
  }

  float const lean_magnitude = 0.12F * amount;
  QVector3D const lean_offset = dir * lean_magnitude;

  m_pose.shoulder_l += lean_offset;
  m_pose.shoulder_r += lean_offset;
  m_pose.neck_base += lean_offset * 0.85F;
  m_pose.head_pos += lean_offset * 0.75F;
}

void HumanoidPoseController::placeHandAt(bool is_left,
                                         const QVector3D &target_position) {

  getHand(is_left) = target_position;

  const QVector3D &shoulder = getShoulder(is_left);
  const QVector3D outward_dir = computeOutwardDir(is_left);

  float const along_frac = is_left ? 0.45F : 0.48F;
  float const lateral_offset = is_left ? 0.15F : 0.12F;
  float const y_bias = is_left ? -0.08F : 0.02F;
  float const outward_sign = 1.0F;

  getElbow(is_left) =
      solveElbowIK(is_left, shoulder, target_position, outward_dir, along_frac,
                   lateral_offset, y_bias, outward_sign);
}

auto HumanoidPoseController::solveElbowIK(
    bool, const QVector3D &shoulder, const QVector3D &hand,
    const QVector3D &outward_dir, float along_frac, float lateral_offset,
    float y_bias, float outward_sign) const -> QVector3D {

  return elbow_bend_torso(shoulder, hand, outward_dir, along_frac, lateral_offset,
                        y_bias, outward_sign);
}

auto HumanoidPoseController::solveKneeIK(
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

  QVector3D bend_pref =
      is_left ? QVector3D(-0.24F, 0.0F, 0.95F) : QVector3D(0.24F, 0.0F, 0.95F);
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

auto HumanoidPoseController::getShoulder(bool is_left) const
    -> const QVector3D & {
  return is_left ? m_pose.shoulder_l : m_pose.shoulder_r;
}

auto HumanoidPoseController::getHand(bool is_left) -> QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto HumanoidPoseController::getHand(bool is_left) const -> const QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto HumanoidPoseController::getElbow(bool is_left) -> QVector3D & {
  return is_left ? m_pose.elbow_l : m_pose.elbow_r;
}

auto HumanoidPoseController::computeRightAxis() const -> QVector3D {
  QVector3D right_axis = m_pose.shoulder_r - m_pose.shoulder_l;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right_axis.normalize();
  return right_axis;
}

auto HumanoidPoseController::computeOutwardDir(bool is_left) const
    -> QVector3D {
  QVector3D const right_axis = computeRightAxis();
  return is_left ? -right_axis : right_axis;
}

auto HumanoidPoseController::get_shoulder_y(bool is_left) const -> float {
  return is_left ? m_pose.shoulder_l.y() : m_pose.shoulder_r.y();
}

auto HumanoidPoseController::get_pelvis_y() const -> float {
  return m_pose.pelvis_pos.y();
}

void HumanoidPoseController::aimBow(float draw_phase) {
  using HP = HumanProportions;

  draw_phase = std::clamp(draw_phase, 0.0F, 1.0F);

  QVector3D const aim_pos(-0.02F, HP::SHOULDER_Y + 0.18F, 0.42F);
  QVector3D const draw_pos(-0.05F, HP::SHOULDER_Y + 0.12F, 0.22F);
  QVector3D const release_pos(-0.02F, HP::SHOULDER_Y + 0.20F, 0.34F);

  QVector3D hand_l_target;
  float shoulder_twist = 0.0F;
  float head_recoil = 0.0F;

  if (draw_phase < 0.20F) {

    float t = draw_phase / 0.20F;
    t = t * t;
    hand_l_target = aim_pos * (1.0F - t) + draw_pos * t;
    shoulder_twist = t * 0.08F;
  } else if (draw_phase < 0.50F) {

    hand_l_target = draw_pos;
    shoulder_twist = 0.08F;
  } else if (draw_phase < 0.58F) {

    float t = (draw_phase - 0.50F) / 0.08F;
    t = t * t * t;
    hand_l_target = draw_pos * (1.0F - t) + release_pos * t;
    shoulder_twist = 0.08F * (1.0F - t * 0.6F);
    head_recoil = t * 0.04F;
  } else {

    float t = (draw_phase - 0.58F) / 0.42F;
    t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_l_target = release_pos * (1.0F - t) + aim_pos * t;
    shoulder_twist = 0.08F * 0.4F * (1.0F - t);
    head_recoil = 0.04F * (1.0F - t);
  }

  QVector3D const hand_r_target(0.03F, HP::SHOULDER_Y + 0.08F, 0.55F);
  placeHandAt(false, hand_r_target);
  placeHandAt(true, hand_l_target);

  if (shoulder_twist > 0.01F) {
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + shoulder_twist);
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_twist * 0.5F);
  }

  if (head_recoil > 0.01F) {
    m_pose.head_pos.setZ(m_pose.head_pos.z() - head_recoil);
  }
}

void HumanoidPoseController::meleeStrike(float strike_phase) {
  using HP = HumanProportions;

  strike_phase = std::clamp(strike_phase, 0.0F, 1.0F);

  QVector3D const rest_pos(0.25F, HP::SHOULDER_Y, 0.10F);
  QVector3D const raised_pos(0.30F, HP::HEAD_TOP_Y + 0.2F, -0.05F);
  QVector3D const strike_pos(0.35F, HP::WAIST_Y, 0.45F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  if (strike_phase < 0.25F) {

    float t = strike_phase / 0.25F;
    t = t * t;
    hand_r_target = rest_pos * (1.0F - t) + raised_pos * t;
    hand_l_target = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * t, 0.20F);
  } else if (strike_phase < 0.35F) {

    hand_r_target = raised_pos;
    hand_l_target = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F, 0.20F);
  } else if (strike_phase < 0.55F) {

    float t = (strike_phase - 0.35F) / 0.2F;
    t = t * t * t;
    hand_r_target = raised_pos * (1.0F - t) + strike_pos * t;
    hand_l_target = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * (1.0F - t * 0.5F),
                              0.20F + 0.15F * t);
  } else {

    float t = (strike_phase - 0.55F) / 0.45F;
    t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_r_target = strike_pos * (1.0F - t) + rest_pos * t;
    hand_l_target = QVector3D(-0.15F, HP::SHOULDER_Y - 0.05F * (1.0F - t),
                              0.35F * (1.0F - t) + 0.20F * t);
  }

  placeHandAt(false, hand_r_target);
  placeHandAt(true, hand_l_target);
}

void HumanoidPoseController::graspTwoHanded(const QVector3D &grip_center,
                                            float hand_separation) {
  hand_separation = std::clamp(hand_separation, 0.1F, 0.8F);

  QVector3D const right_axis = computeRightAxis();

  QVector3D const right_hand_pos =
      grip_center + right_axis * (hand_separation * 0.5F);
  QVector3D const left_hand_pos =
      grip_center - right_axis * (hand_separation * 0.5F);

  placeHandAt(false, right_hand_pos);
  placeHandAt(true, left_hand_pos);
}

void HumanoidPoseController::spearThrust(float attack_phase) {
  using HP = HumanProportions;

  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  QVector3D const guard_pos(0.28F, HP::SHOULDER_Y + 0.05F, 0.25F);
  QVector3D const prepare_pos(0.35F, HP::SHOULDER_Y + 0.08F, 0.05F);
  QVector3D const thrust_pos(0.32F, HP::SHOULDER_Y + 0.10F, 0.90F);
  QVector3D const recover_pos(0.28F, HP::SHOULDER_Y + 0.06F, 0.40F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  auto easeInOutCubic = [](float t) {
    return t < 0.5F ? 4.0F * t * t * t
                    : 1.0F - std::pow(-2.0F * t + 2.0F, 3.0F) / 2.0F;
  };

  auto smoothstep = [](float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };

  auto lerp = [](float a, float b, float t) { return a * (1.0F - t) + b * t; };

  if (attack_phase < 0.20F) {

    float const t = easeInOutCubic(attack_phase / 0.20F);
    hand_r_target = guard_pos * (1.0F - t) + prepare_pos * t;
    hand_l_target = QVector3D(-0.10F, HP::SHOULDER_Y - 0.05F,
                              0.20F * (1.0F - t) + 0.08F * t);
  } else if (attack_phase < 0.30F) {

    hand_r_target = prepare_pos;
    hand_l_target = QVector3D(-0.10F, HP::SHOULDER_Y - 0.05F, 0.08F);
  } else if (attack_phase < 0.50F) {

    float t = (attack_phase - 0.30F) / 0.20F;
    t = t * t * t;
    hand_r_target = prepare_pos * (1.0F - t) + thrust_pos * t;
    hand_l_target =
        QVector3D(-0.10F + 0.05F * t, HP::SHOULDER_Y - 0.05F + 0.03F * t,
                  0.08F + 0.45F * t);
  } else if (attack_phase < 0.70F) {

    float const t = easeInOutCubic((attack_phase - 0.50F) / 0.20F);
    hand_r_target = thrust_pos * (1.0F - t) + recover_pos * t;
    hand_l_target = QVector3D(-0.05F * (1.0F - t) - 0.10F * t,
                              HP::SHOULDER_Y - 0.02F * (1.0F - t) - 0.06F * t,
                              lerp(0.53F, 0.35F, t));
  } else {

    float const t = smoothstep(0.70F, 1.0F, attack_phase);
    hand_r_target = recover_pos * (1.0F - t) + guard_pos * t;
    hand_l_target =
        QVector3D(-0.10F - 0.02F * (1.0F - t),
                  HP::SHOULDER_Y - 0.06F + 0.01F * t, lerp(0.35F, 0.25F, t));
  }

  float const thrust_extent =
      std::clamp((attack_phase - 0.20F) / 0.60F, 0.0F, 1.0F);
  float const along_offset = -0.06F + 0.02F * thrust_extent;
  float const y_drop = 0.10F + 0.02F * thrust_extent;

  hand_l_target = computeOffhandSpearGrip(m_pose, m_anim_ctx, hand_r_target,
                                          false, along_offset, y_drop, -0.08F);

  placeHandAt(false, hand_r_target);
  placeHandAt(true, hand_l_target);
}

void HumanoidPoseController::swordSlash(float attack_phase) {
  using HP = HumanProportions;

  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  QVector3D const rest_pos(0.20F, HP::SHOULDER_Y + 0.05F, 0.15F);
  QVector3D const prepare_pos(0.26F, HP::HEAD_TOP_Y + 0.18F, -0.06F);
  QVector3D const raised_pos(0.25F, HP::HEAD_TOP_Y + 0.22F, 0.02F);
  QVector3D const strike_pos(0.30F, HP::WAIST_Y - 0.05F, 0.50F);
  QVector3D const recover_pos(0.22F, HP::SHOULDER_Y + 0.02F, 0.22F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  auto easeInOutCubic = [](float t) {
    return t < 0.5F ? 4.0F * t * t * t
                    : 1.0F - std::pow(-2.0F * t + 2.0F, 3.0F) / 2.0F;
  };

  auto smoothstep = [](float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };

  auto lerp = [](float a, float b, float t) { return a * (1.0F - t) + b * t; };

  if (attack_phase < 0.18F) {

    float const t = easeInOutCubic(attack_phase / 0.18F);
    hand_r_target = rest_pos * (1.0F - t) + prepare_pos * t;
    hand_l_target =
        QVector3D(-0.21F, HP::SHOULDER_Y - 0.02F - 0.03F * t, 0.15F);
  } else if (attack_phase < 0.32F) {

    float const t = easeInOutCubic((attack_phase - 0.18F) / 0.14F);
    hand_r_target = prepare_pos * (1.0F - t) + raised_pos * t;
    hand_l_target = QVector3D(-0.21F, HP::SHOULDER_Y - 0.05F, 0.17F);
  } else if (attack_phase < 0.52F) {

    float t = (attack_phase - 0.32F) / 0.20F;
    t = t * t * t;
    hand_r_target = raised_pos * (1.0F - t) + strike_pos * t;
    hand_l_target = QVector3D(
        -0.21F, HP::SHOULDER_Y - 0.03F * (1.0F - 0.5F * t), 0.17F + 0.20F * t);
  } else if (attack_phase < 0.72F) {

    float const t = easeInOutCubic((attack_phase - 0.52F) / 0.20F);
    hand_r_target = strike_pos * (1.0F - t) + recover_pos * t;
    hand_l_target = QVector3D(-0.20F, HP::SHOULDER_Y - 0.015F * (1.0F - t),
                              lerp(0.37F, 0.20F, t));
  } else {

    float const t = smoothstep(0.72F, 1.0F, attack_phase);
    hand_r_target = recover_pos * (1.0F - t) + rest_pos * t;
    hand_l_target = QVector3D(-0.20F - 0.02F * (1.0F - t), HP::SHOULDER_Y,
                              lerp(0.20F, 0.15F, t));
  }

  placeHandAt(false, hand_r_target);
  placeHandAt(true, hand_l_target);
}

void HumanoidPoseController::mountOnHorse(float saddle_height) {

  float const offset_y = saddle_height - m_pose.pelvis_pos.y();
  m_pose.pelvis_pos.setY(saddle_height);
}

void HumanoidPoseController::hold_sword_and_shield() {
  using HP = HumanProportions;

  QVector3D const sword_hand_pos(0.30F, HP::SHOULDER_Y - 0.02F, 0.35F);
  QVector3D const shield_hand_pos(-0.22F, HP::SHOULDER_Y, 0.18F);

  placeHandAt(false, sword_hand_pos);
  placeHandAt(true, shield_hand_pos);
}

void HumanoidPoseController::look_at(const QVector3D &target) {
  QVector3D const head_to_target = target - m_pose.head_pos;

  if (head_to_target.lengthSquared() < 1e-6F) {
    return;
  }

  QVector3D const direction = head_to_target.normalized();

  float const max_head_turn = 0.03F;
  QVector3D const head_offset = direction * max_head_turn;

  m_pose.head_pos += QVector3D(head_offset.x(), 0.0F, head_offset.z());

  float const neck_follow = 0.5F;
  m_pose.neck_base += QVector3D(head_offset.x() * neck_follow, 0.0F,
                                head_offset.z() * neck_follow);
}

} // namespace Render::GL
