#include "rigged_mesh.h"

#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLContext>
#include <cstddef>
#include <qopenglext.h>
#include <utility>

namespace Render::GL {

RiggedMesh::RiggedMesh(std::vector<RiggedVertex> vertices,
                       std::vector<std::uint32_t> indices)
    : m_vertices(std::move(vertices)), m_indices(std::move(indices)) {}

RiggedMesh::~RiggedMesh() = default;

void RiggedMesh::setup_buffers() {
  if (QOpenGLContext::currentContext() == nullptr) {
    qWarning() << "RiggedMesh::setup_buffers called without current GL "
                  "context; skipping VAO/VBO creation";
    return;
  }
  initializeOpenGLFunctions();

  m_vao = std::make_unique<VertexArray>();
  m_vbo = std::make_unique<Buffer>(Buffer::Type::Vertex);
  m_ebo = std::make_unique<Buffer>(Buffer::Type::Index);

  m_vao->bind();
  m_vbo->set_data(m_vertices);
  m_ebo->set_data(m_indices);

  // The shared `VertexArray::add_vertexBuffer` helper only handles
  // float attribs via glVertexAttribPointer. RiggedVertex needs an
  // integer attribute (bone_indices, GL_UNSIGNED_BYTE -> ivec4 via
  // glVertexAttribIPointer), so we configure the layout manually.
  constexpr GLsizei k_stride = sizeof(RiggedVertex);
  constexpr auto offset_of = [](auto member_ptr) -> std::size_t {
    return reinterpret_cast<std::size_t>(
        &(reinterpret_cast<RiggedVertex const *>(0)->*member_ptr));
  };
  auto const pos_off = offset_of(&RiggedVertex::position_bone_local);
  auto const norm_off = offset_of(&RiggedVertex::normal_bone_local);
  auto const tex_off = offset_of(&RiggedVertex::tex_coord);
  auto const bi_off = offset_of(&RiggedVertex::bone_indices);
  auto const bw_off = offset_of(&RiggedVertex::bone_weights);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, k_stride,
                        reinterpret_cast<void *>(pos_off));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, k_stride,
                        reinterpret_cast<void *>(norm_off));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, k_stride,
                        reinterpret_cast<void *>(tex_off));
  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, k_stride,
                         reinterpret_cast<void *>(bi_off));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, k_stride,
                        reinterpret_cast<void *>(bw_off));

  m_vao->unbind();

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "RiggedMesh::setup_buffers GL error" << err;
  }
}

auto RiggedMesh::bind_vao() -> bool {
  if (!m_vao) {
    setup_buffers();
  }
  if (!m_vao || QOpenGLContext::currentContext() == nullptr) {
    return false;
  }
  m_vao->bind();
  return true;
}

auto RiggedMesh::ensure_gl_buffers() -> bool {
  if (!m_vao) {
    setup_buffers();
  }
  return m_vbo != nullptr && m_ebo != nullptr;
}

void RiggedMesh::unbind_vao() {
  if (m_vao) {
    m_vao->unbind();
  }
}

void RiggedMesh::draw() {
  if (!bind_vao()) {
    return;
  }
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                 GL_UNSIGNED_INT, nullptr);
  m_vao->unbind();

#ifndef NDEBUG
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "RiggedMesh::draw GL error" << err << "indices"
               << m_indices.size();
  }
#endif
}

} // namespace Render::GL
