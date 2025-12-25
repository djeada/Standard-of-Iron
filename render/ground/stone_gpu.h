#pragma once

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

struct StoneInstanceGpu {
  QVector4D pos_scale;
  QVector4D color_rot;
};

struct StoneBatchParams {
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;

  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }

  QVector3D light_direction = default_light_direction();
  float time = 0.0F;
};

} 
