#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <cstdint>
#include <span>

#include "render/gl/backend/mesh_buffers.h"

namespace Render::GL::BackendPipelines {

struct VertexAttributeLayout {
  GLuint location{0};
  GLint components{3};
  std::size_t offset{0};
};

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
