#pragma once

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

struct GrassInstanceGpu {
  QVector4D posHeight{0.0F, 0.0F, 0.0F, 0.0F};
  QVector4D colorWidth{0.0F, 0.0F, 0.0F, 0.0F};
  QVector4D swayParams{0.0F, 0.0F, 0.0F, 0.0F};
};

struct GrassBatchParams {
  static constexpr float kDefaultSoilColorR = 0.28F;
  static constexpr float kDefaultSoilColorG = 0.24F;
  static constexpr float kDefaultSoilColorB = 0.18F;
  static constexpr float kDefaultWindStrength = 0.25F;
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultWindSpeed = 1.4F;

  static auto defaultSoilColor() -> QVector3D {
    return {kDefaultSoilColorR, kDefaultSoilColorG, kDefaultSoilColorB};
  }

  static auto defaultLightDirection() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D soilColor = defaultSoilColor();
  float windStrength{kDefaultWindStrength};
  QVector3D light_direction = defaultLightDirection();
  float windSpeed{kDefaultWindSpeed};
  float time{0.0F};
  float pad0{0.0F};
  float pad1{0.0F};
  float pad2{0.0F};
};

} // namespace Render::GL
