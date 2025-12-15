#pragma once

#include "../gl/mesh.h"
#include <memory>

namespace Render::Geom {

// Mode type constants for rendering
constexpr int k_mode_type_hold = 0;
constexpr int k_mode_type_guard = 1;

class ModeIndicator {
public:
  static auto get_hold_mode_mesh() -> Render::GL::Mesh *;
  static auto get_guard_mode_mesh() -> Render::GL::Mesh *;

private:
  static auto create_hold_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;
  static auto create_guard_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;

  static std::unique_ptr<Render::GL::Mesh> s_hold_mesh;
  static std::unique_ptr<Render::GL::Mesh> s_guard_mesh;
};

} // namespace Render::Geom
