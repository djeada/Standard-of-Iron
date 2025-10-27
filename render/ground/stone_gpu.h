#pragma once

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

struct StoneInstanceGpu {
  QVector4D posScale;
  QVector4D colorRot;
};

struct StoneBatchParams {
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;

  static auto defaultLightDirection() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D light_direction = defaultLightDirection();
  float time = 0.0F;
};

} // namespace Render::GL
