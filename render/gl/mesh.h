#pragma once

#include "buffer.h"
#include <QOpenGLFunctions_3_3_Core>
#include <memory>
#include <vector>

namespace Render::GL {

struct Vertex {
  float position[3];
  float normal[3];
  float texCoord[2];
};

class Mesh : protected QOpenGLFunctions_3_3_Core {
public:
  Mesh(const std::vector<Vertex> &vertices,
       const std::vector<unsigned int> &indices);
  ~Mesh();

  void draw();

  const std::vector<Vertex> &getVertices() const { return m_vertices; }
  const std::vector<unsigned int> &getIndices() const { return m_indices; }

private:
  std::vector<Vertex> m_vertices;
  std::vector<unsigned int> m_indices;

  std::unique_ptr<VertexArray> m_vao;
  std::unique_ptr<Buffer> m_vbo;
  std::unique_ptr<Buffer> m_ebo;

  void setupBuffers();
};

Mesh *createQuadMesh();
Mesh *createCubeMesh();
Mesh *createPlaneMesh(float width, float height, int subdivisions = 1);

} 