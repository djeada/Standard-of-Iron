#pragma once

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

struct GrassInstanceGpu {
  QVector4D posHeight{0.0f, 0.0f, 0.0f, 0.0f};
  QVector4D colorWidth{0.0f, 0.0f, 0.0f, 0.0f};
  QVector4D swayParams{0.0f, 0.0f, 0.0f, 0.0f};
};

struct GrassBatchParams {
  QVector3D soilColor{0.28f, 0.24f, 0.18f};
  float windStrength{0.25f};
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};
  float windSpeed{1.4f};
  float time{0.0f};
  float pad0{0.0f};
  float pad1{0.0f};
  float pad2{0.0f};
};

} 
