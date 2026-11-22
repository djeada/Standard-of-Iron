#include "render/geom/shared_geometry_cache.h"

#include <utility>

#include "render/geom/mesh.h"

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

void SharedGeometryCache::for_each_mesh(const MeshVisitor& visit) const {
  std::lock_guard<std::mutex> const lock(m_mutex);
  for (const auto& [key, mesh] : m_meshes) {
    visit(*mesh);
  }
}

auto SharedGeometryCache::size() const -> std::size_t {
  std::lock_guard<std::mutex> const lock(m_mutex);
  return m_meshes.size();
}

} // namespace Render::GL
