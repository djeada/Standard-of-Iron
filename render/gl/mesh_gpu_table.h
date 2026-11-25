#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "render/gl/buffer.h"

namespace Render::GL {

class Mesh;

class MeshGpuTable : protected QOpenGLFunctions_3_3_Core {
public:
  MeshGpuTable();
  ~MeshGpuTable() override;

  MeshGpuTable(const MeshGpuTable&) = delete;
  auto operator=(const MeshGpuTable&) -> MeshGpuTable& = delete;
  MeshGpuTable(MeshGpuTable&&) = delete;
  auto operator=(MeshGpuTable&&) -> MeshGpuTable& = delete;

  auto bind(const Mesh& mesh) -> bool;
  void unbind();

  void draw_bound(const Mesh& mesh, std::size_t instance_count = 1);
  void draw(const Mesh& mesh);

  [[nodiscard]] auto claim_instance_layout(const Mesh& mesh) -> bool;

  auto prewarm(const Mesh& mesh) -> bool;

  void collect_retired();

  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_entries.size(); }

private:
  struct Entry {
    std::weak_ptr<const void> lifetime;
    std::unique_ptr<VertexArray> vao;
    std::unique_ptr<Buffer> vbo;
    std::unique_ptr<Buffer> ebo;
    bool instance_layout_ready = false;
  };

  auto upload(const Mesh& mesh, Entry& entry) -> bool;
  auto prepare(const Mesh& mesh, const char* caller_name) -> Entry*;

  std::unordered_map<std::uint64_t, Entry> m_entries;

  std::uint64_t m_last_id = 0;
  Entry* m_last = nullptr;
  bool m_functions_ready = false;
};

} // namespace Render::GL
