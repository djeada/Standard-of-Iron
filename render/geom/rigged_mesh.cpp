#include "render/geom/rigged_mesh.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <utility>

#include "render/profiling/asset_counters.h"

namespace Render::GL {

auto RiggedMesh::next_id() noexcept -> std::uint64_t {
  static std::atomic<std::uint64_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

RiggedMesh::RiggedMesh(std::vector<RiggedVertex> vertices,
                       std::vector<std::uint32_t> indices)
    : m_vertices(std::move(vertices))
    , m_indices(std::move(indices))
    , m_lifetime(std::make_shared<const int>(0)) {
  Render::Profiling::count_asset(
      Render::Profiling::AssetCounter::RiggedMeshConstructed);
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::RiggedMeshVertexBytes,
                                 (m_vertices.size() * sizeof(RiggedVertex)) +
                                     (m_indices.size() * sizeof(std::uint32_t)));
  const auto vertex_is_rigid = [this](std::uint32_t index) {
    if (index >= m_vertices.size()) {
      return false;
    }
    const auto& weights = m_vertices[index].bone_weights;
    return weights[0] == 1.0F && weights[1] == 0.0F && weights[2] == 0.0F &&
           weights[3] == 0.0F;
  };
  const auto triangle_is_rigid = [&](std::size_t offset) {
    return vertex_is_rigid(m_indices[offset]) &&
           vertex_is_rigid(m_indices[offset + 1U]) &&
           vertex_is_rigid(m_indices[offset + 2U]);
  };

  std::vector<std::uint32_t> partitioned;
  partitioned.reserve(m_indices.size());
  const std::size_t triangle_index_count = m_indices.size() - m_indices.size() % 3U;
  for (std::size_t offset = 0; offset < triangle_index_count; offset += 3U) {
    if (triangle_is_rigid(offset)) {
      partitioned.insert(partitioned.end(),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset + 3U));
    }
  }
  m_rigid_index_count = partitioned.size();
  for (std::size_t offset = 0; offset < triangle_index_count; offset += 3U) {
    if (!triangle_is_rigid(offset)) {
      partitioned.insert(partitioned.end(),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset + 3U));
    }
  }
  partitioned.insert(partitioned.end(),
                     m_indices.begin() +
                         static_cast<std::ptrdiff_t>(triangle_index_count),
                     m_indices.end());
  m_indices = std::move(partitioned);
}

RiggedMesh::~RiggedMesh() = default;

} // namespace Render::GL
