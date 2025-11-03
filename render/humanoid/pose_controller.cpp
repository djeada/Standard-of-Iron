#include "pose_controller.h"
#include "humanoid_math.h"
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

  // No-op if depth is zero
  if (depth < 1e-6F) {
    return;
  }

  float const kneel_offset = depth * 0.40F;
  float const pelvis_y = HP::WAIST_Y - kneel_offset;
  m_pose.pelvisPos.setY(pelvis_y);

  float const stance_narrow = 0.11F;

  float const left_knee_y = HP::GROUND_Y + 0.07F * depth;
  float const left_knee_z = -0.06F * depth;
  m_pose.knee_l = QVector3D(-stance_narrow, left_knee_y, left_knee_z);
  m_pose.footL = QVector3D(-stance_narrow - 0.025F, HP::GROUND_Y,
                           left_knee_z - HP::LOWER_LEG_LEN * 0.93F * depth);

  float const right_knee_y = pelvis_y - 0.12F;
  float const right_foot_z = 0.28F * depth;
  m_pose.knee_r = QVector3D(stance_narrow, right_knee_y, right_foot_z - 0.05F);
  m_pose.foot_r =
      QVector3D(stance_narrow, HP::GROUND_Y + m_pose.footYOffset, right_foot_z);

  float const upper_body_drop = kneel_offset;
  m_pose.shoulderL.setY(m_pose.shoulderL.y() - upper_body_drop);
  m_pose.shoulderR.setY(m_pose.shoulderR.y() - upper_body_drop);
  m_pose.neck_base.setY(m_pose.neck_base.y() - upper_body_drop);
  m_pose.headPos.setY(m_pose.headPos.y() - upper_body_drop);
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

  m_pose.shoulderL += lean_offset;
  m_pose.shoulderR += lean_offset;
  m_pose.neck_base += lean_offset * 0.85F;
  m_pose.headPos += lean_offset * 0.75F;
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

  return elbowBendTorso(shoulder, hand, outward_dir, along_frac, lateral_offset,
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

  float const knee_floor = HP::GROUND_Y + m_pose.footYOffset * 0.5F;
  if (knee.y() < knee_floor) {
    knee.setY(knee_floor);
  }

  // Ensure knee is not above hip
  if (knee.y() > hip.y()) {
    knee.setY(hip.y());
  }

  return knee;
}

auto HumanoidPoseController::getShoulder(bool is_left) const
    -> const QVector3D & {
  return is_left ? m_pose.shoulderL : m_pose.shoulderR;
}

auto HumanoidPoseController::getHand(bool is_left) -> QVector3D & {
  return is_left ? m_pose.handL : m_pose.hand_r;
}

auto HumanoidPoseController::getHand(bool is_left) const -> const QVector3D & {
  return is_left ? m_pose.handL : m_pose.hand_r;
}

auto HumanoidPoseController::getElbow(bool is_left) -> QVector3D & {
  return is_left ? m_pose.elbowL : m_pose.elbowR;
}

auto HumanoidPoseController::computeRightAxis() const -> QVector3D {
  QVector3D right_axis = m_pose.shoulderR - m_pose.shoulderL;
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

} // namespace Render::GL
