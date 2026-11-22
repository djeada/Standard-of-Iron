

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {

struct RiggedVertex {
  std::array<float, 3> position_bone_local{};
  std::array<float, 3> normal_bone_local{};
  std::array<float, 2> tex_coord{};
  std::array<std::uint8_t, 4> bone_indices{};
  std::array<float, 4> bone_weights{};
  std::uint8_t color_role{0};
  std::array<std::uint8_t, 3> padding{};
};

// A skinned mesh: bone-local vertices, indices with the rigidly skinned
// triangles first, and a process-unique id. It holds no GL object; each backend
// uploads its own copy through its RiggedMeshGpuTable, keyed by id(), and drops
// it once the mesh is destroyed.
class RiggedMesh {
public:
  RiggedMesh(std::vector<RiggedVertex> vertices, std::vector<std::uint32_t> indices);
  ~RiggedMesh();

  RiggedMesh(const RiggedMesh&) = delete;
  auto operator=(const RiggedMesh&) -> RiggedMesh& = delete;
  RiggedMesh(RiggedMesh&&) = delete;
  auto operator=(RiggedMesh&&) -> RiggedMesh& = delete;

  [[nodiscard]] auto id() const noexcept -> std::uint64_t { return m_id; }
  [[nodiscard]] auto lifetime() const noexcept -> std::weak_ptr<const void> {
    return m_lifetime;
  }

  [[nodiscard]] auto get_vertices() const noexcept -> const std::vector<RiggedVertex>& {
    return m_vertices;
  }
  [[nodiscard]] auto get_indices() const noexcept -> const std::vector<std::uint32_t>& {
    return m_indices;
  }
  [[nodiscard]] auto vertex_count() const noexcept -> std::size_t {
    return m_vertices.size();
  }
  [[nodiscard]] auto index_count() const noexcept -> std::size_t {
    return m_indices.size();
  }
  [[nodiscard]] auto rigid_index_count() const noexcept -> std::size_t {
    return m_rigid_index_count;
  }
  [[nodiscard]] auto rigid_skinning() const noexcept -> bool {
    return !m_indices.empty() && m_rigid_index_count == m_indices.size();
  }

private:
  std::uint64_t m_id = next_id();
  static auto next_id() noexcept -> std::uint64_t;
  std::vector<RiggedVertex> m_vertices;
  std::vector<std::uint32_t> m_indices;
  std::size_t m_rigid_index_count{0};
  std::shared_ptr<const int> m_lifetime;
};

} // namespace Render::GL
