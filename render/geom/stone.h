#pragma once
#include <QMatrix4x4>
#include <QVector3D>

#include "../gl/mesh.h"

namespace Render {
namespace Geom {

class Stone {
public:
  static auto get() -> GL::Mesh*;
};

} // namespace Geom
} // namespace Render

#include "projectile_renderer.h"
