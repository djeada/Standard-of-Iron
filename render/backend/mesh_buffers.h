#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include "render/gl/gl_handle.h"

namespace Render::GL::BackendPipelines {

struct StaticMeshBuffers {
  VertexArrayHandle vao;
  BufferHandle vertex_buffer;
  BufferHandle index_buffer;
  BufferHandle instance_buffer;
  GLsizei vertex_count{0};
  GLsizei index_count{0};

  [[nodiscard]] auto drawable() const -> bool { return vao.valid() && index_count > 0; }

  void reset() { *this = StaticMeshBuffers{}; }
};

} // namespace Render::GL::BackendPipelines
