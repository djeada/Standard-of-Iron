#pragma once

#include "buffer.h"
#include <QOpenGLFunctions_3_3_Core>
#include <array>
#include <memory>
#include <vector>

namespace Render::GL {

struct Vertex {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> tex_coord{};
};

class Mesh : protected QOpenGLFunctions_3_3_Core {
public:
  Mesh(const std::vector<Vertex> &vertices,
       const std::vector<unsigned int> &indices);
  ~Mesh() override;

  void draw();

  [[nodiscard]] auto getVertices() const -> const std::vector<Vertex> & {
    return m_vertices;
  }
  [[nodiscard]] auto getIndices() const -> const std::vector<unsigned int> & {
    return m_indices;
  }

private:
  std::vector<Vertex> m_vertices;
  std::vector<unsigned int> m_indices;

  std::unique_ptr<VertexArray> m_vao;
  std::unique_ptr<Buffer> m_vbo;
  std::unique_ptr<Buffer> m_ebo;

  void setupBuffers();
};

auto createQuadMesh() -> Mesh *;
auto createCubeMesh() -> Mesh *;
auto createPlaneMesh(float width, float height, int subdivisions = 1) -> Mesh *;

} // namespace Render::GL
