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
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};
  float time = 0.0f;
  float windStrength = 0.25f;
  float windSpeed = 1.4f;
  float pad0 = 0.0f;
  float pad1 = 0.0f;
};

} 
