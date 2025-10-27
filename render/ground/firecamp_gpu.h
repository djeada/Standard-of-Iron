#pragma once

#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

struct FireCampInstanceGpu {
  QVector4D pos_intensity;
  QVector4D radius_phase;
};

struct FireCampBatchParams {
  static constexpr float kDefaultFlickerSpeed = 5.0F;
  static constexpr float kDefaultFlickerAmount = 0.02F;
  static constexpr float kDefaultGlowStrength = 1.25F;

  float time = 0.0F;
  float flickerSpeed = kDefaultFlickerSpeed;
  float flickerAmount = kDefaultFlickerAmount;
  float glowStrength = kDefaultGlowStrength;
};

} // namespace Render::GL
