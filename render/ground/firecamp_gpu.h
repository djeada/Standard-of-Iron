#pragma once

#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

struct FireCampInstanceGpu {
  QVector4D posIntensity;  // x, y, z, intensity
  QVector4D radiusPhase;   // radius, phase, duration (or 1.0 for persistent), unused
};

struct FireCampBatchParams {
  float time = 0.0f;
  float flickerSpeed = 5.0f;
  float flickerAmount = 0.02f;
  float glowStrength = 1.25f;
};

} // namespace Render::GL
