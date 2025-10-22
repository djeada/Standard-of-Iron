#pragma once

#include "../gl/mesh.h"
#include <memory>

namespace Render::Geom {

class SelectionDisc {
public:
  static Render::GL::Mesh *get();

private:
  static std::unique_ptr<Render::GL::Mesh> s_mesh;
};

} 
