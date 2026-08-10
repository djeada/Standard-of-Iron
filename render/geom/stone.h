#pragma once
#include <QMatrix4x4>
#include <QVector3D>

#include "render/gl/mesh.h"

namespace Render {
namespace Geom {

class Stone {
public:
  static auto get() -> GL::Mesh*;

  static constexpr float k_mean_radius = 1.0F;
  static constexpr float k_projectile_radius = 0.15F;
};

} // namespace Geom
} // namespace Render

#include "projectile_renderer.h"
