#pragma once

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

struct PlantInstanceGpu {
  QVector4D posScale;   // xyz=world pos, w=scale
  QVector4D colorSway;  // rgb=tint color, a=sway phase
  QVector4D typeParams; // x=plant type, y=rotation, z=sway strength, w=sway
                        // speed
};

struct PlantBatchParams {
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};
  float time = 0.0f;
  float windStrength = 0.25f;
  float windSpeed = 1.4f;
  float pad0 = 0.0f;
  float pad1 = 0.0f;
};

} // namespace Render::GL
