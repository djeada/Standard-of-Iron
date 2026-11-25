#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "render/gl/buffer.h"
#include "render/gl/rigged_mesh_gpu.h"

namespace Render::GL {

class RiggedMesh;

class RiggedMeshGpuTable {
public:
  RiggedMeshGpuTable();
  ~RiggedMeshGpuTable();

  RiggedMeshGpuTable(const RiggedMeshGpuTable&) = delete;
  auto operator=(const RiggedMeshGpuTable&) -> RiggedMeshGpuTable& = delete;
  RiggedMeshGpuTable(RiggedMeshGpuTable&&) = delete;
  auto operator=(RiggedMeshGpuTable&&) -> RiggedMeshGpuTable& = delete;

  auto ensure(const RiggedMesh& mesh) -> bool;

  void draw(const RiggedMesh& mesh);

  [[nodiscard]] auto vertex_buffer(const RiggedMesh& mesh) -> Buffer*;
  [[nodiscard]] auto index_buffer(const RiggedMesh& mesh) -> Buffer*;

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

  std::uint64_t m_last_id = 0;
  Entry* m_last = nullptr;
};

} // namespace Render::GL
