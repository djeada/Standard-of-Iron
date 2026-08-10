#include "render/gl/backend/static_mesh_upload.h"

namespace Render::GL::BackendPipelines {

void upload_static_instanced_mesh(QOpenGLFunctions_3_3_Core& gl,
                                  StaticMeshBuffers& mesh,
                                  const void* vertex_data,
                                  std::size_t vertex_count,
                                  std::size_t vertex_stride,
                                  std::span<const VertexAttributeLayout> attributes,
                                  const void* index_data,
                                  std::size_t index_count,
                                  std::span<const GLuint> instance_locations) {

  gl.glGenVertexArrays(1, &mesh.vao);
  gl.glBindVertexArray(mesh.vao);

  gl.glGenBuffers(1, &mesh.vertex_buffer);
  gl.glBindBuffer(GL_ARRAY_BUFFER, mesh.vertex_buffer);
  gl.glBufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(vertex_count * vertex_stride),
                  vertex_data,
                  GL_STATIC_DRAW);
  mesh.vertex_count = static_cast<GLsizei>(vertex_count);

  for (const auto& attribute : attributes) {
    gl.glEnableVertexAttribArray(attribute.location);
    gl.glVertexAttribPointer(attribute.location,
                             attribute.components,
                             GL_FLOAT,
                             GL_FALSE,
                             static_cast<GLsizei>(vertex_stride),
                             reinterpret_cast<void*>(attribute.offset));
  }

  gl.glGenBuffers(1, &mesh.index_buffer);
  gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.index_buffer);
  gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(index_count * sizeof(std::uint16_t)),
                  index_data,
                  GL_STATIC_DRAW);
  mesh.index_count = static_cast<GLsizei>(index_count);

  for (const GLuint location : instance_locations) {
    gl.glEnableVertexAttribArray(location);
    gl.glVertexAttribDivisor(location, 1);
  }

  gl.glBindVertexArray(0);
  gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
  gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace Render::GL::BackendPipelines
