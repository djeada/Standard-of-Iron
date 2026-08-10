#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <cstdint>
#include <span>

#include "render/gl/backend/mesh_buffers.h"

namespace Render::GL::BackendPipelines {

// One per-vertex attribute in an interleaved vertex buffer.
struct VertexAttributeLayout {
  GLuint location{0};
  GLint components{3};
  std::size_t offset{0};
};

// Uploads an interleaved, indexed static mesh into `mesh`, wires up its vertex
// layout, and enables `instance_locations` at divisor 1 for the per-instance
// buffer the pipeline binds later.
//
// Every instanced prop pipeline used to open-code this same fifty-line block.
// The instance locations are a parameter rather than a fixed 3/4/5 because the
// scatter shaders do not agree on them: the foliage shaders start their
// instance data at location 3, stone starts at 2.
void upload_static_instanced_mesh(QOpenGLFunctions_3_3_Core& gl,
                                  StaticMeshBuffers& mesh,
                                  const void* vertex_data,
                                  std::size_t vertex_count,
                                  std::size_t vertex_stride,
                                  std::span<const VertexAttributeLayout> attributes,
                                  const void* index_data,
                                  std::size_t index_count,
                                  std::span<const GLuint> instance_locations);

} // namespace Render::GL::BackendPipelines
