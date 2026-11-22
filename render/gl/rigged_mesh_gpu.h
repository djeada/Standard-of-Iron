#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <span>

#include "gl_handle.h"

namespace Render::GL {

class RiggedMeshGpu : protected QOpenGLFunctions_3_3_Core {
public:
  void initialize();

  [[nodiscard]] auto drain_gl_errors() -> unsigned int;
  [[nodiscard]] auto current_gl_error() -> unsigned int;

  void configure_vertex_attributes();
  void draw_triangles(std::size_t index_count);
};

[[nodiscard]] auto palette_ubo_available() -> bool;

[[nodiscard]] auto
create_palette_ubo(std::span<const float> palette_floats) -> BufferHandle;

} // namespace Render::GL
