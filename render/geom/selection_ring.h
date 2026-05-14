#pragma once

#include <memory>

#include "../gl/mesh.h"

namespace Render::Geom {

class SelectionRing {
public:
  static auto get() -> Render::GL::Mesh*;

private:
  static std::unique_ptr<Render::GL::Mesh> s_mesh;
};

} // namespace Render::Geom
