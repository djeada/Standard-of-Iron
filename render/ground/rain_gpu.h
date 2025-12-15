#pragma once

#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

struct RainDropInstanceGpu {
  QVector4D pos_velocity;
  QVector4D size_alpha;
};

struct RainBatchParams {
  static constexpr float k_default_drop_speed = 15.0F;
  static constexpr float k_default_drop_length = 0.8F;
  static constexpr float k_default_drop_width = 0.02F;

  float time = 0.0F;
  float intensity = 0.5F;
  float drop_speed = k_default_drop_speed;
  float drop_length = k_default_drop_length;
  float drop_width = k_default_drop_width;
  float wind_strength = 0.0F;
  QVector3D wind_direction{1.0F, 0.0F, 0.3F};
};

} // namespace Render::GL
