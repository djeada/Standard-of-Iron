#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace Render::GL {

struct Vertex {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> tex_coord{};
  std::uint8_t color_role{0};
  std::array<std::uint8_t, 3> padding{};

  std::array<std::uint8_t, 4> bone_indices{};
  std::array<float, 4> bone_weights{};
};

// Geometry a renderer can draw: vertices, indices and bounds. A Mesh holds no
// GL objects; each backend uploads its own copy through its MeshGpuTable, keyed
// by id(), and drops it once the Mesh is destroyed.
class Mesh {
public:
  Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
  ~Mesh();

  Mesh(const Mesh&) = delete;
  auto operator=(const Mesh&) -> Mesh& = delete;
  Mesh(Mesh&&) = delete;
  auto operator=(Mesh&&) -> Mesh& = delete;

  [[nodiscard]] auto id() const noexcept -> std::uint64_t { return m_id; }
  [[nodiscard]] auto lifetime() const noexcept -> std::weak_ptr<const void> {
    return m_lifetime;
  }

  [[nodiscard]] auto get_vertices() const -> const std::vector<Vertex>& {
    return m_vertices;
  }
  [[nodiscard]] auto get_indices() const -> const std::vector<unsigned int>& {
    return m_indices;
  }
  [[nodiscard]] auto bounds_center() const noexcept -> const QVector3D& {
    return m_bounds_center;
  }
  [[nodiscard]] auto bounds_radius() const noexcept -> float { return m_bounds_radius; }

  [[nodiscard]] auto clone_with_filtered_indices(
      const std::function<
          bool(unsigned int, unsigned int, unsigned int, const std::vector<Vertex>&)>&
          predicate) const -> std::unique_ptr<Mesh> {
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
  QVector3D m_bounds_center;
  float m_bounds_radius = 0.0F;
  std::uint64_t m_id;
  std::shared_ptr<const int> m_lifetime;
};

auto create_quad_mesh() -> std::unique_ptr<Mesh>;
auto create_cube_mesh() -> std::unique_ptr<Mesh>;
auto create_plane_mesh(float width,
                       float height,
                       int subdivisions = 1) -> std::unique_ptr<Mesh>;

} // namespace Render::GL
