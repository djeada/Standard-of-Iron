#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace Render::GL {

class Mesh;

// Stable 64-bit name for a cached mesh. Callers that build one mesh per name
// pass just the name; callers that build a family of meshes from parameters
// fold those into `variant` so each parameter set gets its own entry.
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

// Process-wide store for the immutable meshes that many renderers share: the
// selection disc, the arrow parts, the unit primitives and so on. They are
// built once, on first use, and they own GL objects, so the cache owns them
// rather than leaving them in function-local statics that outlive the context.
// Backend::~Backend calls release_all() while the context is still current.
class SharedGeometryCache {
public:
  using Builder = std::function<std::unique_ptr<Mesh>()>;

  [[nodiscard]] static auto instance() -> SharedGeometryCache&;

  // Returns the mesh for `key`, invoking `build` the first time it is asked
  // for. A builder that returns nullptr is retried on the next call.
  [[nodiscard]] auto get_or_build(std::uint64_t key, const Builder& build) -> Mesh*;

  // Drops every cached mesh. Must run with a current GL context.
  void release_all();

  [[nodiscard]] auto size() const -> std::size_t;

private:
  SharedGeometryCache() = default;

  mutable std::mutex m_mutex;
  std::unordered_map<std::uint64_t, std::unique_ptr<Mesh>> m_meshes;
};

} // namespace Render::GL
