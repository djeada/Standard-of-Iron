#pragma once

#include <QMatrix4x4>
#include <QString>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Render {

struct MeshHandle {
  std::uint32_t id{0};
  [[nodiscard]] auto valid() const -> bool { return id != 0; }

  bool operator==(const MeshHandle &o) const { return id == o.id; }
  bool operator!=(const MeshHandle &o) const { return id != o.id; }
};

struct MeshHandleHash {
  auto operator()(const MeshHandle &h) const -> std::size_t {
    return std::hash<std::uint32_t>{}(h.id);
  }
};

struct PooledVertex {
  float position[3];
  float normal[3];
  float texcoord[2];
};

struct PooledMeshInfo {
  std::uint32_t vao{0};
  std::uint32_t vbo{0};
  std::uint32_t ebo{0};
  std::uint32_t vertex_count{0};
  std::uint32_t index_count{0};
  bool uploaded{false};
};

class MeshGeometryPool {
public:
  static constexpr std::uint32_t k_invalid_handle = 0;
  static constexpr std::size_t k_max_pooled_meshes = 1024;

  MeshGeometryPool() = default;
  ~MeshGeometryPool() = default;

  MeshGeometryPool(const MeshGeometryPool &) = delete;
  auto operator=(const MeshGeometryPool &) -> MeshGeometryPool & = delete;
  MeshGeometryPool(MeshGeometryPool &&) = delete;
  auto operator=(MeshGeometryPool &&) -> MeshGeometryPool & = delete;

  auto register_mesh(const QString &name, std::vector<PooledVertex> vertices,
                     std::vector<std::uint32_t> indices) -> MeshHandle {
    std::lock_guard lock(m_mutex);
    auto it = m_name_to_handle.find(name);
    if (it != m_name_to_handle.end()) {
      return it->second;
    }

    MeshHandle handle{++m_next_id};
    PooledMeshEntry entry;
    entry.name = name;
    entry.vertices = std::move(vertices);
    entry.indices = std::move(indices);
    entry.info.vertex_count = static_cast<std::uint32_t>(entry.vertices.size());
    entry.info.index_count = static_cast<std::uint32_t>(entry.indices.size());

    m_entries[handle.id] = std::move(entry);
    m_name_to_handle[name] = handle;
    return handle;
  }

  void upload_pending() {
    std::lock_guard lock(m_mutex);
    for (auto &[id, entry] : m_entries) {
      if (!entry.info.uploaded && !entry.vertices.empty()) {
        upload_entry(entry);
      }
    }
  }

  [[nodiscard]] auto get(MeshHandle handle) const -> const PooledMeshInfo * {
    std::lock_guard lock(m_mutex);
    auto it = m_entries.find(handle.id);
    return (it != m_entries.end()) ? &it->second.info : nullptr;
  }

  [[nodiscard]] auto find(const QString &name) const -> MeshHandle {
    std::lock_guard lock(m_mutex);
    auto it = m_name_to_handle.find(name);
    return (it != m_name_to_handle.end()) ? it->second : MeshHandle{};
  }

  [[nodiscard]] auto size() const -> std::size_t {
    std::lock_guard lock(m_mutex);
    return m_entries.size();
  }

  [[nodiscard]] auto uploaded_count() const -> std::size_t {
    std::lock_guard lock(m_mutex);
    std::size_t count = 0;
    for (const auto &[id, entry] : m_entries) {
      if (entry.info.uploaded)
        ++count;
    }
    return count;
  }

  void release_gpu_resources();

  struct PoolStats {
    std::size_t total_meshes{0};
    std::size_t uploaded_meshes{0};
    std::size_t total_vertices{0};
    std::size_t total_indices{0};
    std::size_t gpu_memory_bytes{0};
  };

  [[nodiscard]] auto stats() const -> PoolStats {
    std::lock_guard lock(m_mutex);
    PoolStats s;
    s.total_meshes = m_entries.size();
    for (const auto &[id, entry] : m_entries) {
      s.total_vertices += entry.vertices.size();
      s.total_indices += entry.indices.size();
      if (entry.info.uploaded) {
        ++s.uploaded_meshes;
        s.gpu_memory_bytes += entry.vertices.size() * sizeof(PooledVertex) +
                              entry.indices.size() * sizeof(std::uint32_t);
      }
    }
    return s;
  }

private:
  struct PooledMeshEntry {
    QString name;
    std::vector<PooledVertex> vertices;
    std::vector<std::uint32_t> indices;
    PooledMeshInfo info;
  };

  void upload_entry(PooledMeshEntry &entry);

  mutable std::mutex m_mutex;
  std::uint32_t m_next_id{0};
  std::unordered_map<std::uint32_t, PooledMeshEntry> m_entries;
  std::unordered_map<QString, MeshHandle> m_name_to_handle;
};

} // namespace Render
