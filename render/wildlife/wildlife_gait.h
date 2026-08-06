#pragma once

#include <QVector3D>

#include "wildlife_rig.h"

namespace Render::Wildlife {

// One gait's footfall geometry, in model units.
//
// `stride` is how far a paw travels backwards along the ground while it is planted,
// and `stance_duty` is the fraction of the cycle it stays planted. Together they fix
// how far the body advances per cycle (`gait_advance`), which is the number the
// animation rate has to be driven from: a cycle per that much ground travel is the
// only cadence at which planted feet do not slide.
struct GaitPlan {
  float stride{0.0F};
  float stance_duty{0.5F};
  float lift{0.0F};
  // Fore-aft travel of the hip itself. A quadruped's scapula and pelvis ride with the
  // limb, which is where most of a stride longer than the leg can swing comes from.
  float hip_swing{0.0F};
};

[[nodiscard]] constexpr auto gait_advance(const GaitPlan& plan) noexcept -> float {
  return plan.stance_duty > 0.0F ? plan.stride / plan.stance_duty : 0.0F;
}

// A leg's standing pose plus the lengths derived from it. Build one with
// `make_leg_rest` so the segment lengths and the knee's bend direction always come
// from the authored silhouette and cannot drift away from it.
struct LegRest {
  QVector3D hip{};
  QVector3D knee{};
  QVector3D ankle{};
  QVector3D toe{};
  QVector3D pastern{};
  float phase_offset{0.0F};
  float upper{0.0F};
  float lower{0.0F};
  float bend{1.0F};
};

[[nodiscard]] auto make_leg_rest(const QVector3D& hip,
                                 const QVector3D& knee,
                                 const QVector3D& ankle,
                                 const QVector3D& toe,
                                 float phase_offset) noexcept -> LegRest;

// Places the paw on its stance/swing path and solves the two bones above it, so the
// paw tracks the ground in a straight line instead of swinging through it on an arc.
// `weight` fades the whole gait out towards the standing pose.
void solve_leg(const LegRest& rest,
               const GaitPlan& plan,
               float cycle_phase,
               float weight,
               LegJoints& out) noexcept;

} // namespace Render::Wildlife
