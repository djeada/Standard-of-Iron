#pragma once

#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

struct PineInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_sway;
  QVector4D rotation;
};

struct PineBatchParams {
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultWindStrength = 0.3F;
  static constexpr float kDefaultWindSpeed = 0.5F;

  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
  float wind_strength = kDefaultWindStrength;
  float wind_speed = kDefaultWindSpeed;
};

} 
