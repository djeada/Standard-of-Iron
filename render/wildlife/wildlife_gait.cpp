#include "wildlife_gait.h"

#include <algorithm>
#include <cmath>

namespace Render::Wildlife {

namespace {

constexpr float k_pi = 3.14159265F;

auto sagittal_normal(const QVector3D& dir) noexcept -> QVector3D {
  return {0.0F, dir.z(), -dir.y()};
}

} // namespace

auto make_leg_rest(const QVector3D& hip,
                   const QVector3D& knee,
                   const QVector3D& ankle,
                   const QVector3D& toe,
                   float phase_offset) noexcept -> LegRest {
  LegRest rest;
  rest.hip = hip;
  rest.knee = knee;
  rest.ankle = ankle;
  rest.toe = toe;
  rest.pastern = ankle - toe;
  rest.phase_offset = phase_offset;
  rest.upper = (knee - hip).length();
  rest.lower = (ankle - knee).length();

  QVector3D const span = ankle - hip;
  float const len = span.length();
  if (len > 1.0e-5F) {

    float const side = QVector3D::dotProduct(knee - hip, sagittal_normal(span / len));
    rest.bend = side < 0.0F ? -1.0F : 1.0F;
  }
  return rest;
}

void solve_leg(const LegRest& rest,
               const GaitPlan& plan,
               float cycle_phase,
               float weight,
               LegJoints& out) noexcept {
  float const t = cycle_phase - std::floor(cycle_phase);
  float const duty = std::clamp(plan.stance_duty, 0.05F, 0.95F);

  float travel = 0.0F;
  float lift = 0.0F;
  if (t < duty) {
    travel = (0.5F - (t / duty)) * plan.stride;
  } else {
    float const u = (t - duty) / (1.0F - duty);

    float const eased = u * u * (3.0F - (2.0F * u));
    travel = (eased - 0.5F) * plan.stride;

    float const bell = std::sin(u * k_pi);
    lift = plan.lift * bell * bell;
  }

  float const w = std::clamp(weight, 0.0F, 1.0F);
  float const reach_fraction = plan.stride > 1.0e-5F ? (travel / plan.stride) : 0.0F;

  QVector3D const hip =
      rest.hip + QVector3D(0.0F, 0.0F, reach_fraction * plan.hip_swing * w);
  QVector3D toe = rest.toe + QVector3D(0.0F, lift * w, travel * w);
  QVector3D ankle = toe + rest.pastern;

  QVector3D span = ankle - hip;
  float len = span.length();
  float const reach = (rest.upper + rest.lower) * 0.999F;
  if (len > reach) {
    span *= reach / len;
    len = reach;
    ankle = hip + span;
    toe = ankle - rest.pastern;
  }
  len = std::max(len, 1.0e-4F);

  QVector3D const dir = span / len;
  float const along =
      ((len * len) + (rest.upper * rest.upper) - (rest.lower * rest.lower)) /
      (2.0F * len);
  float const across =
      std::sqrt(std::max(0.0F, (rest.upper * rest.upper) - (along * along)));

  out.shoulder = hip;
  out.knee = hip + (dir * along) + (sagittal_normal(dir) * (across * rest.bend));
  out.foot = ankle;
  out.toe = toe;
}

auto capture_skeleton_lengths(const RigPose& pose) noexcept -> SkeletonLengths {
  SkeletonLengths held;
  held.tail_mid = (pose.tail_mid - pose.tail_base).length();
  held.tail_tip = (pose.tail_tip - pose.tail_mid).length();
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    held.legs[i][0] = (pose.legs[i].knee - pose.legs[i].shoulder).length();
    held.legs[i][1] = (pose.legs[i].foot - pose.legs[i].knee).length();
    held.legs[i][2] = (pose.legs[i].toe - pose.legs[i].foot).length();
  }
  return held;
}

void hold_bone(const QVector3D& parent, QVector3D& child, float length) noexcept {
  QVector3D const bone = child - parent;
  float const current = bone.length();
  if (current > 1.0e-5F && length > 1.0e-5F) {
    child = parent + ((bone / current) * length);
  }
}

void enforce_skeleton_lengths(RigPose& pose, const SkeletonLengths& held) noexcept {
  hold_bone(pose.tail_base, pose.tail_mid, held.tail_mid);
  hold_bone(pose.tail_mid, pose.tail_tip, held.tail_tip);
  for (std::size_t i = 0; i < k_leg_count; ++i) {
    hold_bone(pose.legs[i].shoulder, pose.legs[i].knee, held.legs[i][0]);
    hold_bone(pose.legs[i].knee, pose.legs[i].foot, held.legs[i][1]);
    hold_bone(pose.legs[i].foot, pose.legs[i].toe, held.legs[i][2]);
  }
}

} // namespace Render::Wildlife
