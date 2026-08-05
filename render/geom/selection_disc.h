#pragma once

#include <memory>

#include "../gl/mesh.h"

namespace Render::Geom {

class SelectionDisc {
public:
  static auto get() -> Render::GL::Mesh*;

private:
  static auto mesh() -> std::unique_ptr<Render::GL::Mesh>&;
};

} // namespace Render::Geom
