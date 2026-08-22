#pragma once

#include <QVector3D>

namespace Render::GL {

enum class LegIndex : int {
  FrontLeft = 0,
  FrontRight = 1,
  RearLeft = 2,
  RearRight = 3
};

struct ElephantLegState {
  QVector3D planted_foot{0.0F, 0.0F, 0.0F};
  QVector3D swing_start{0.0F, 0.0F, 0.0F};
  QVector3D swing_target{0.0F, 0.0F, 0.0F};
  float swing_progress = 0.0F;
  bool in_swing = false;
};

struct ElephantGaitState {
  ElephantLegState legs[4]{};
  float cycle_phase = 0.0F;
  float weight_shift_x = 0.0F;
  float weight_shift_z = 0.0F;
  float shoulder_lag = 0.0F;
  float hip_lag = 0.0F;
  bool initialized = false;
};

struct ElephantLegPose {
  QVector3D hip;
  QVector3D knee;
  QVector3D ankle;
  QVector3D foot;
  float upper_radius;
  float lower_radius;
};

} // namespace Render::GL
