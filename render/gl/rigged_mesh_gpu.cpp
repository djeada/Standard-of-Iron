#include "render/gl/rigged_mesh_gpu.h"

#include <QOpenGLContext>

#include <cstddef>
#include <utility>

#include "render/geom/rigged_mesh.h"
#include "render/gl/platform_gl.h"

namespace Render::GL {

void RiggedMeshGpu::initialize() {
  initializeOpenGLFunctions();
}

auto RiggedMeshGpu::drain_gl_errors() -> unsigned int {
  unsigned int first = GL_NO_ERROR;
  for (GLenum stale = glGetError(); stale != GL_NO_ERROR; stale = glGetError()) {
    if (first == GL_NO_ERROR) {
      first = stale;
    }
  }
  return first;
}

auto RiggedMeshGpu::current_gl_error() -> unsigned int {
  return glGetError();
}

void RiggedMeshGpu::configure_vertex_attributes() {
  constexpr GLsizei k_stride = sizeof(RiggedVertex);
  constexpr auto offset_of = [](auto member_ptr) -> std::size_t {
    return reinterpret_cast<std::size_t>(
        &(reinterpret_cast<RiggedVertex const*>(0)->*member_ptr));
  };
  auto const pos_off = offset_of(&RiggedVertex::position_bone_local);
  auto const norm_off = offset_of(&RiggedVertex::normal_bone_local);
  auto const tex_off = offset_of(&RiggedVertex::tex_coord);
  auto const bi_off = offset_of(&RiggedVertex::bone_indices);
  auto const bw_off = offset_of(&RiggedVertex::bone_weights);
  auto const role_off = offset_of(&RiggedVertex::color_role);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      0, 3, GL_FLOAT, GL_FALSE, k_stride, reinterpret_cast<void*>(pos_off));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 3, GL_FLOAT, GL_FALSE, k_stride, reinterpret_cast<void*>(norm_off));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
      2, 2, GL_FLOAT, GL_FALSE, k_stride, reinterpret_cast<void*>(tex_off));
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(
      3, 4, GL_UNSIGNED_BYTE, k_stride, reinterpret_cast<void*>(bi_off));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(
      4, 4, GL_FLOAT, GL_FALSE, k_stride, reinterpret_cast<void*>(bw_off));
  glEnableVertexAttribArray(5);
  glVertexAttribIPointer(
      5, 1, GL_UNSIGNED_BYTE, k_stride, reinterpret_cast<void*>(role_off));
}

void RiggedMeshGpu::draw_triangles(std::size_t index_count) {
  glDrawElements(
      GL_TRIANGLES, static_cast<GLsizei>(index_count), GL_UNSIGNED_INT, nullptr);
}

} // namespace Render::GL
