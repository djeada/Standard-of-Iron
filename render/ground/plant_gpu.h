#pragma once

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

struct PlantInstanceGpu {
  QVector4D posScale;
  QVector4D colorSway;
  QVector4D typeParams;
};

struct PlantBatchParams {
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultWindStrength = 0.25F;
  static constexpr float kDefaultWindSpeed = 1.4F;

  static auto defaultLightDirection() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D light_direction = defaultLightDirection();
  float time = 0.0F;
  float windStrength = kDefaultWindStrength;
  float windSpeed = kDefaultWindSpeed;
  float pad0 = 0.0F;
  float pad1 = 0.0F;
};

} // namespace Render::GL
