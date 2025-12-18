#pragma once

#include "../gl/mesh.h"
#include <memory>

namespace Render::Geom {

class SelectionRing {
public:
  static auto get() -> Render::GL::Mesh *;

private:
  static std::unique_ptr<Render::GL::Mesh> s_mesh;
};

} // namespace Render::Geom
