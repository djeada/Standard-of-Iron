#pragma once

#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

struct PineInstanceGpu {
  QVector4D posScale;
  QVector4D colorSway;
  QVector4D rotation;
};

struct PineBatchParams {
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};
  float time = 0.0f;
  float windStrength = 0.3f;
  float windSpeed = 0.5f;
};

} 
