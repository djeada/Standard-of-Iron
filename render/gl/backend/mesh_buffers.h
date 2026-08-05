#pragma once

#include <QOpenGLFunctions_3_3_Core>

namespace Render::GL::BackendPipelines {

struct StaticMeshBuffers {
  GLuint vao{0};
  GLuint vertex_buffer{0};
  GLuint index_buffer{0};
  GLuint instance_buffer{0};
  GLsizei vertex_count{0};
  GLsizei index_count{0};

  [[nodiscard]] auto drawable() const -> bool { return vao != 0U && index_count > 0; }
};

void release_mesh_buffers(QOpenGLFunctions_3_3_Core& gl, StaticMeshBuffers& mesh);

} // namespace Render::GL::BackendPipelines
