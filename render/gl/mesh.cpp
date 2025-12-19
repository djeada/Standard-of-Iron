#include "mesh.h"
#include "gl/buffer.h"
#include "render_constants.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLFunctions_3_3_Core>
#include <memory>
#include <vector>

namespace Render::GL {

using namespace Render::GL::ComponentCount;

Mesh::Mesh(const std::vector<Vertex> &vertices,
           const std::vector<unsigned int> &indices)
    : m_vertices(vertices), m_indices(indices) {}

Mesh::~Mesh() = default;

void Mesh::setup_buffers() {
  if (QOpenGLContext::currentContext() == nullptr) {
    qWarning() << "Mesh::setup_buffers called without current GL context; "
                  "skipping VAO/VBO creation";
    return;
  }
  initializeOpenGLFunctions();
  m_vao = std::make_unique<VertexArray>();
  m_vbo = std::make_unique<Buffer>(Buffer::Type::Vertex);
  m_ebo = std::make_unique<Buffer>(Buffer::Type::Index);

  m_vao->bind();

  m_vbo->set_data(m_vertices);

  m_ebo->set_data(m_indices);

  std::vector<int> const layout = {Vec3, Vec3, Vec2};
  m_vao->add_vertexBuffer(*m_vbo, layout);
  m_vao->set_index_buffer(*m_ebo);

  m_vao->unbind();

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "Mesh::setup_buffers GL error" << err;
  }
}

void Mesh::draw() {
  if (!m_vao) {
    setup_buffers();
  }
  if (QOpenGLContext::currentContext() == nullptr) {
    qWarning() << "Mesh::draw called without current GL context; skipping draw"
               << "indices" << m_indices.size();
    return;
  }
  m_vao->bind();

  initializeOpenGLFunctions();
  GLenum preErr = glGetError();
  if (preErr != GL_NO_ERROR) {
    qWarning() << "Mesh::draw pre-draw GL error" << preErr << "vao"
               << (m_vao ? m_vao->id() : 0) << "indices" << m_indices.size();
  }
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                 GL_UNSIGNED_INT, nullptr);

  m_vao->unbind();

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "Mesh::draw GL error" << err << "indices" << m_indices.size();
  }
}

auto create_quad_mesh() -> std::unique_ptr<Mesh> {
  std::vector<Vertex> const vertices = {

      {{-1.0F, -1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F}},
      {{1.0F, -1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F}},
      {{1.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F}},
      {{-1.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F}}};

  std::vector<unsigned int> const indices = {0, 1, 2, 2, 3, 0};

  return std::make_unique<Mesh>(vertices, indices);
}

auto create_cube_mesh() -> std::unique_ptr<Mesh> {
  std::vector<Vertex> const vertices = {

      {{-1.0F, -1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F}},
      {{1.0F, -1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F}},
      {{1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F}},
      {{-1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F}},

      {{-1.0F, -1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F}},
      {{-1.0F, 1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {1.0F, 1.0F}},
      {{1.0F, 1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 1.0F}},
      {{1.0F, -1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F}},
  };

  std::vector<unsigned int> const indices = {0, 1, 2, 2, 3, 0,

                                             4, 5, 6, 6, 7, 4,

                                             4, 0, 3, 3, 5, 4,

                                             1, 7, 6, 6, 2, 1,

                                             3, 2, 6, 6, 5, 3,

                                             4, 7, 1, 1, 0, 4};

  return std::make_unique<Mesh>(vertices, indices);
}

auto create_plane_mesh(float width, float height,
                     int subdivisions) -> std::unique_ptr<Mesh> {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  float const half_width = width * 0.5F;
  float const half_height = height * 0.5F;

  for (int z = 0; z <= subdivisions; ++z) {
    for (int x = 0; x <= subdivisions; ++x) {
      float x_pos = (x / static_cast<float>(subdivisions)) * width - half_width;
      float z_pos =
          (z / static_cast<float>(subdivisions)) * height - half_height;

      vertices.push_back({{x_pos, 0.0F, z_pos},
                          {0.0F, 1.0F, 0.0F},
                          {x / static_cast<float>(subdivisions),
                           z / static_cast<float>(subdivisions)}});
    }
  }

  for (int z = 0; z < subdivisions; ++z) {
    for (int x = 0; x < subdivisions; ++x) {
      int top_left = z * (subdivisions + 1) + x;
      int top_right = top_left + 1;
      int bottom_left = (z + 1) * (subdivisions + 1) + x;
      int bottom_right = bottom_left + 1;

      indices.push_back(top_left);
      indices.push_back(bottom_left);
      indices.push_back(top_right);

      indices.push_back(top_right);
      indices.push_back(bottom_left);
      indices.push_back(bottom_right);
    }
  }

  return std::make_unique<Mesh>(vertices, indices);
}

} // namespace Render::GL
