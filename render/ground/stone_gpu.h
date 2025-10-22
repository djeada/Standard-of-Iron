#pragma once

#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

struct StoneInstanceGpu {
  QVector4D posScale;
  QVector4D colorRot;
};

struct StoneBatchParams {
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};
  float time = 0.0f;
};

} // namespace Render::GL
