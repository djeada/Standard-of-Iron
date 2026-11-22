#pragma once

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "render/gl/buffer.h"

namespace Render::GL {

class Mesh;

// The GPU copies of the meshes one backend draws. A Mesh is geometry only; the
// first time a table binds it, the table uploads a vertex array and buffers on
// its own context and keeps them until the Mesh is destroyed
// (collect_retired()) or the table is. Two backends therefore never share or
// fight over a vertex array, and destroying a backend releases every mesh it
// uploaded.
class MeshGpuTable : protected QOpenGLFunctions_3_3_Core {
public:
  MeshGpuTable();
  ~MeshGpuTable() override;

  MeshGpuTable(const MeshGpuTable&) = delete;
  auto operator=(const MeshGpuTable&) -> MeshGpuTable& = delete;
  MeshGpuTable(MeshGpuTable&&) = delete;
  auto operator=(MeshGpuTable&&) -> MeshGpuTable& = delete;

  // Binds the mesh's vertex array, uploading it first if this table has none.
  auto bind(const Mesh& mesh) -> bool;
  void unbind();

  // Draws the bound mesh; bind() it first.
  void draw_bound(const Mesh& mesh, std::size_t instance_count = 1);
  void draw(const Mesh& mesh);

  // True once per uploaded vertex array: the caller sets up its instance
  // attributes then.
  [[nodiscard]] auto claim_instance_layout(const Mesh& mesh) -> bool;

  // Uploads without drawing. False if there is no context to upload into.
  auto prewarm(const Mesh& mesh) -> bool;

  // Releases the GPU copies of meshes that no longer exist.
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
};

} // namespace Render::GL
