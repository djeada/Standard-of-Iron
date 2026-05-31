#include "pose_primitives.h"

#include <algorithm>
#include <cmath>

#include "../gl/humanoid/humanoid_types.h"
#include "humanoid_math.h"

namespace Render::Humanoid::PosePrimitives {

auto solve_elbow_ik(const QVector3D& shoulder,
                    const QVector3D& hand,
                    const QVector3D& outward_dir,
                    float along_frac,
                    float lateral_offset,
                    float y_bias,
                    float outward_sign) -> QVector3D {
  return Render::GL::elbow_bend_torso(
      shoulder, hand, outward_dir, along_frac, lateral_offset, y_bias, outward_sign);
}

auto solve_knee_ik(const QVector3D& hip,
                   const QVector3D& foot,
                   const KneeIkParams& params) -> QVector3D {
  QVector3D const hip_to_foot = foot - hip;
  float const distance = hip_to_foot.length();
  if (distance < 1e-5F) {
    return hip;
  }

  float const reach = params.upper_leg_len + params.lower_leg_len;
  float const min_reach =
      std::max(std::abs(params.upper_leg_len - params.lower_leg_len) + 1e-4F, 1e-3F);
  float const max_reach = std::max(reach - 1e-4F, min_reach + 1e-4F);
  float const clamped_dist = std::clamp(distance, min_reach, max_reach);

  QVector3D const dir = hip_to_foot / distance;

  float cos_theta =
      (params.upper_leg_len * params.upper_leg_len + clamped_dist * clamped_dist -
       params.lower_leg_len * params.lower_leg_len) /
      (2.0F * params.upper_leg_len * clamped_dist);
  cos_theta = std::clamp(cos_theta, -1.0F, 1.0F);
  float const sin_theta = std::sqrt(std::max(0.0F, 1.0F - cos_theta * cos_theta));

  QVector3D bend_pref = params.bend_preference;
  if (bend_pref.lengthSquared() < 1e-6F) {
    bend_pref = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    bend_pref.normalize();
  }

  QVector3D bend_axis = bend_pref - dir * QVector3D::dotProduct(dir, bend_pref);
  if (bend_axis.lengthSquared() < 1e-6F) {
    bend_axis = QVector3D::crossProduct(dir, QVector3D(0.0F, 1.0F, 0.0F));
    if (bend_axis.lengthSquared() < 1e-6F) {
      bend_axis = QVector3D::crossProduct(dir, QVector3D(1.0F, 0.0F, 0.0F));
    }
  }
  bend_axis.normalize();

  QVector3D knee = hip + dir * (cos_theta * params.upper_leg_len) +
                   bend_axis * (sin_theta * params.upper_leg_len);

  if (knee.y() < params.knee_floor) {
    knee.setY(params.knee_floor);
  }

  if (params.clamp_to_hip_height && knee.y() > hip.y()) {
    knee.setY(hip.y());
  }

  return knee;
}

auto compute_right_axis(const Render::GL::HumanoidPose& pose) -> QVector3D {
  QVector3D right_axis = pose.shoulder_r - pose.shoulder_l;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right_axis.normalize();
  return right_axis;
}

auto compute_outward_dir(const Render::GL::HumanoidPose& pose,
                         Render::GL::Side side) -> QVector3D {
  QVector3D const right_axis = compute_right_axis(pose);
  return (side == Render::GL::Side::Left) ? -right_axis : right_axis;
}

} // namespace Render::Humanoid::PosePrimitives
