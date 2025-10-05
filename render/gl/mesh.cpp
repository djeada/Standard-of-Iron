#include "mesh.h"
#include <QOpenGLFunctions_3_3_Core>

namespace Render::GL {

Mesh::Mesh(const std::vector<Vertex> &vertices,
           const std::vector<unsigned int> &indices)
    : m_vertices(vertices), m_indices(indices) {}

Mesh::~Mesh() = default;

void Mesh::setupBuffers() {
  initializeOpenGLFunctions();
  m_vao = std::make_unique<VertexArray>();
  m_vbo = std::make_unique<Buffer>(Buffer::Type::Vertex);
  m_ebo = std::make_unique<Buffer>(Buffer::Type::Index);

  m_vao->bind();

  m_vbo->setData(m_vertices);

  m_ebo->setData(m_indices);

  std::vector<int> layout = {3, 3, 2};
  m_vao->addVertexBuffer(*m_vbo, layout);
  m_vao->setIndexBuffer(*m_ebo);

  m_vao->unbind();
}

void Mesh::draw() {
  if (!m_vao) {
    setupBuffers();
  }
  m_vao->bind();

  initializeOpenGLFunctions();
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                 GL_UNSIGNED_INT, nullptr);

  m_vao->unbind();
}

Mesh *createQuadMesh() {
  std::vector<Vertex> vertices = {

      {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};

  std::vector<unsigned int> indices = {0, 1, 2, 2, 3, 0};

  return new Mesh(vertices, indices);
}

Mesh *createCubeMesh() {
  std::vector<Vertex> vertices = {

      {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

      {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
      {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
      {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
      {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
  };

  std::vector<unsigned int> indices = {0, 1, 2, 2, 3, 0,

                                       4, 5, 6, 6, 7, 4,

                                       4, 0, 3, 3, 5, 4,

                                       1, 7, 6, 6, 2, 1,

                                       3, 2, 6, 6, 5, 3,

                                       4, 7, 1, 1, 0, 4};

  return new Mesh(vertices, indices);
}

Mesh *createPlaneMesh(float width, float height, int subdivisions) {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  float halfWidth = width * 0.5f;
  float halfHeight = height * 0.5f;

  for (int z = 0; z <= subdivisions; ++z) {
    for (int x = 0; x <= subdivisions; ++x) {
      float xPos = (x / static_cast<float>(subdivisions)) * width - halfWidth;
      float zPos = (z / static_cast<float>(subdivisions)) * height - halfHeight;

      vertices.push_back({{xPos, 0.0f, zPos},
                          {0.0f, 1.0f, 0.0f},
                          {x / static_cast<float>(subdivisions),
                           z / static_cast<float>(subdivisions)}});
    }
  }

  for (int z = 0; z < subdivisions; ++z) {
    for (int x = 0; x < subdivisions; ++x) {
      int topLeft = z * (subdivisions + 1) + x;
      int topRight = topLeft + 1;
      int bottomLeft = (z + 1) * (subdivisions + 1) + x;
      int bottomRight = bottomLeft + 1;

      indices.push_back(topLeft);
      indices.push_back(bottomLeft);
      indices.push_back(topRight);

      indices.push_back(topRight);
      indices.push_back(bottomLeft);
      indices.push_back(bottomRight);
    }
  }

  return new Mesh(vertices, indices);
}

} // namespace Render::GL