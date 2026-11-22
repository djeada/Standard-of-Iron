#include "render/gl/shared_geometry_cache.h"

#include <QOpenGLContext>

#include <utility>

#include "render/gl/mesh.h"

namespace Render::GL {

auto SharedGeometryCache::instance() -> SharedGeometryCache& {
  static SharedGeometryCache cache;
  return cache;
}

auto SharedGeometryCache::get_or_build(std::uint64_t key,
                                       const Builder& build) -> Mesh* {
  std::lock_guard<std::mutex> const lock(m_mutex);
  auto it = m_meshes.find(key);
  if (it != m_meshes.end()) {
    return it->second.get();
  }
  auto mesh = build ? build() : nullptr;
  if (mesh == nullptr) {
    return nullptr;
  }
  return m_meshes.emplace(key, std::move(mesh)).first->second.get();
}

auto SharedGeometryCache::prewarm_gpu_resources(MeshGpuTable& meshes) -> bool {
  std::lock_guard<std::mutex> const lock(m_mutex);
  bool ready = true;
  for (auto& [key, mesh] : m_meshes) {
    ready = meshes.prewarm(*mesh) && ready;
  }
  return ready;
}

auto SharedGeometryCache::size() const -> std::size_t {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_meshes.size();
}

} // namespace Render::GL
