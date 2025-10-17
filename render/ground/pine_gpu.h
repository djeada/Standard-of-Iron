#pragma once

#include <QVector3D>
#include <QVector4D>
#include <cstdint>

namespace Render::GL {

// GPU instance data for pine trees
struct PineInstanceGpu {
  QVector4D posScale;    // xyz = world position, w = scale multiplier
  QVector4D colorSway;   // rgb = tint color, a = sway phase offset
  QVector4D rotation;    // x = Y-axis rotation, yzw = reserved for future use
};

// Parameters for pine tree batch rendering
struct PineBatchParams {
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};
  float time = 0.0f;
  float windStrength = 0.3f;  // Gentler than plants
  float windSpeed = 0.5f;     // Slower than plants
};

} // namespace Render::GL
