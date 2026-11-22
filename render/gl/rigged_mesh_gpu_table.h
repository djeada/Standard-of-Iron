#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "render/gl/buffer.h"
#include "render/gl/rigged_mesh_gpu.h"

namespace Render::GL {

class RiggedMesh;

// The GPU copies of the skinned meshes one backend draws. A RiggedMesh holds
// geometry only; the first time a table needs it, the table uploads a vertex
// array and buffers on its own context and keeps them until the mesh is
// destroyed (collect_retired()) or the table is.
class RiggedMeshGpuTable {
public:
  RiggedMeshGpuTable();
  ~RiggedMeshGpuTable();

  RiggedMeshGpuTable(const RiggedMeshGpuTable&) = delete;
  auto operator=(const RiggedMeshGpuTable&) -> RiggedMeshGpuTable& = delete;
  RiggedMeshGpuTable(RiggedMeshGpuTable&&) = delete;
  auto operator=(RiggedMeshGpuTable&&) -> RiggedMeshGpuTable& = delete;

  // Uploads the mesh if this table has no copy yet. False without a context.
  auto ensure(const RiggedMesh& mesh) -> bool;

  // Draws every index of the mesh with its own vertex array.
  void draw(const RiggedMesh& mesh);

  // The uploaded buffers, for pipelines that bind them as storage; null until
  // ensure() succeeded.
  [[nodiscard]] auto vertex_buffer(const RiggedMesh& mesh) -> Buffer*;
  [[nodiscard]] auto index_buffer(const RiggedMesh& mesh) -> Buffer*;

  // Releases the copies of meshes that no longer exist.
  void collect_retired();

  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_entries.size(); }

private:
  struct Entry {
    std::weak_ptr<const void> lifetime;
    std::unique_ptr<VertexArray> vao;
    std::unique_ptr<Buffer> vbo;
    std::unique_ptr<Buffer> ebo;
  };

  auto entry_for(const RiggedMesh& mesh) -> Entry*;

  RiggedMeshGpu m_gpu;
  bool m_gpu_ready = false;
  std::unordered_map<std::uint64_t, Entry> m_entries;
  // The last entry looked up. Draws of one mesh or block sit together in the
  // sorted queue, so most lookups hit it instead of hashing. Entry addresses
  // are stable in the map; anything that erases clears it.
  std::uint64_t m_last_id = 0;
  Entry* m_last = nullptr;
};

} // namespace Render::GL
