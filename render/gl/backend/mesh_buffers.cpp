#include "mesh_buffers.h"

#include <QOpenGLContext>

namespace Render::GL::BackendPipelines {

void release_mesh_buffers(QOpenGLFunctions_3_3_Core& gl, StaticMeshBuffers& mesh) {

  if (QOpenGLContext::currentContext() == nullptr) {
    mesh = StaticMeshBuffers{};
    return;
  }

  if (mesh.instance_buffer != 0U) {
    gl.glDeleteBuffers(1, &mesh.instance_buffer);
  }
  if (mesh.index_buffer != 0U) {
    gl.glDeleteBuffers(1, &mesh.index_buffer);
  }
  if (mesh.vertex_buffer != 0U) {
    gl.glDeleteBuffers(1, &mesh.vertex_buffer);
  }
  if (mesh.vao != 0U) {
    gl.glDeleteVertexArrays(1, &mesh.vao);
  }
  mesh = StaticMeshBuffers{};
}

} // namespace Render::GL::BackendPipelines
