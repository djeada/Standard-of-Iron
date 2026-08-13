#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "render/gl/backend/mesh_buffers.h"
#include "render/gl/render_constants.h"

namespace Render::GL::BackendPipelines {

struct VertexAttributeLayout {
  GLuint location{0};
  GLint components{3};
  std::size_t offset{0};
};

inline constexpr std::array<VertexAttributeLayout, 3> k_position_normal_texcoord_layout{
    {
        {static_cast<GLuint>(VertexAttrib::position), ComponentCount::vec3, 0},
        {static_cast<GLuint>(VertexAttrib::normal),
         ComponentCount::vec3,
         sizeof(float) * 3},
        {static_cast<GLuint>(VertexAttrib::tex_coord),
         ComponentCount::vec2,
         sizeof(float) * 6},
    }};

void upload_static_instanced_mesh(QOpenGLFunctions_3_3_Core& gl,
                                  StaticMeshBuffers& mesh,
                                  const void* vertex_data,
                                  std::size_t vertex_count,
                                  std::size_t vertex_stride,
                                  std::span<const VertexAttributeLayout> attributes,
                                  const void* index_data,
                                  std::size_t index_count,
                                  std::span<const GLuint> instance_locations);

auto upload_static_effect_mesh(QOpenGLFunctions_3_3_Core& gl,
                               StaticMeshBuffers& mesh,
                               const char* label,
                               const void* vertex_data,
                               std::size_t vertex_count,
                               std::size_t vertex_stride,
                               std::span<const VertexAttributeLayout> attributes,
                               std::span<const unsigned int> indices) -> bool;

} // namespace Render::GL::BackendPipelines
