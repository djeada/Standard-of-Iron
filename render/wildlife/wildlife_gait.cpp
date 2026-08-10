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

} // namespace Render::Wildlife
