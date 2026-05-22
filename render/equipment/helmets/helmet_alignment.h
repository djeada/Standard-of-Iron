#pragma once

#include <QVector3D>

namespace Render::GL {

// Helmet local offsets are scaled by the head frame radius, so they need to be
// fairly large to create a visible in-game placement change.
inline constexpr QVector3D k_helmet_local_offset{0.0F, 0.225F, 0.0F};
inline constexpr float k_helmet_uniform_scale = 0.8F;

} // namespace Render::GL
