#pragma once
#include "../gl/mesh.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
namespace Geom {

class Stone {
public:
  static auto get() -> GL::Mesh *;
};

} 
} 

#include "projectile_renderer.h"
