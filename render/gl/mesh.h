#pragma once

#include "buffer.h"
#include <QOpenGLFunctions_3_3_Core>
#include <array>
#include <functional>
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

  [[nodiscard]] auto cloneWithFilteredIndices(
      const std::function<bool(unsigned int, unsigned int, unsigned int,
                               const std::vector<Vertex> &)> &predicate) const
      -> std::unique_ptr<Mesh> {
    if (!predicate) {
      return nullptr;
    }

    std::vector<unsigned int> filtered;
    filtered.reserve(m_indices.size());
    for (std::size_t i = 0; i + 2 < m_indices.size(); i += 3) {
      unsigned int a = m_indices[i];
      unsigned int b = m_indices[i + 1];
      unsigned int c = m_indices[i + 2];
      if (!predicate(a, b, c, m_vertices)) {
        filtered.push_back(a);
        filtered.push_back(b);
        filtered.push_back(c);
      }
    }

    if (filtered.empty()) {
      return nullptr;
    }

    return std::make_unique<Mesh>(m_vertices, filtered);
  }

private:
  std::vector<Vertex> m_vertices;
  std::vector<unsigned int> m_indices;

  std::unique_ptr<VertexArray> m_vao;
  std::unique_ptr<Buffer> m_vbo;
  std::unique_ptr<Buffer> m_ebo;

  void setupBuffers();
};

auto createQuadMesh() -> std::unique_ptr<Mesh>;
auto createCubeMesh() -> std::unique_ptr<Mesh>;
auto createPlaneMesh(float width, float height, int subdivisions = 1) -> std::unique_ptr<Mesh>;

} // namespace Render::GL
