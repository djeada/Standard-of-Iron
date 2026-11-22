#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include "render/geom/mesh.h"

namespace Render::GL {

[[nodiscard]] constexpr auto
geometry_key(std::string_view name,
             std::uint64_t variant = 0) noexcept -> std::uint64_t {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const char c : name) {
    hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
    hash *= 1099511628211ULL;
  }
  hash ^= variant + 0x9E3779B97F4A7C15ULL + (hash << 6U) + (hash >> 2U);
  return hash;
}

class SharedGeometryCache {
public:
  using Builder = std::function<std::unique_ptr<Mesh>()>;

  [[nodiscard]] static auto instance() -> SharedGeometryCache&;

  [[nodiscard]] auto get_or_build(std::uint64_t key, const Builder& build) -> Mesh*;

  // Visits every cached mesh, under the cache lock; used to prewarm a
  // backend's MeshGpuTable.
  void for_each_mesh(const MeshVisitor& visit) const;

  [[nodiscard]] auto size() const -> std::size_t;

private:
  SharedGeometryCache() = default;

  mutable std::mutex m_mutex;
  std::unordered_map<std::uint64_t, std::unique_ptr<Mesh>> m_meshes;
};

} // namespace Render::GL
