#pragma once

#include <QVector3D>

#include "wildlife_rig.h"

namespace Render::Wildlife {

struct GaitPlan {
  float stride{0.0F};
  float stance_duty{0.5F};
  float lift{0.0F};

  float hip_swing{0.0F};
};

[[nodiscard]] constexpr auto gait_advance(const GaitPlan& plan) noexcept -> float {
  return plan.stance_duty > 0.0F ? plan.stride / plan.stance_duty : 0.0F;
}

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

void solve_leg(const LegRest& rest,
               const GaitPlan& plan,
               float cycle_phase,
               float weight,
               LegJoints& out) noexcept;

} // namespace Render::Wildlife
