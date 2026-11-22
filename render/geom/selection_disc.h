#pragma once

#include <memory>

#include "render/geom/mesh.h"

namespace Render::Geom {

class SelectionDisc {
public:
  static auto get() -> Render::GL::Mesh*;
};

} // namespace Render::Geom
