#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Render::GL {

// Per-instance data a draw command points at: grass blades, stones, fog puffs.
// It holds bytes only, no GL object. A domain renderer owns and fills it; each
// backend keeps its own vertex buffer for it in an InstanceBufferTable, keyed
// by id(), uploads the bytes again whenever version() moves, and drops the
// buffer once the block is destroyed.
class InstanceBlock {
public:
  InstanceBlock();
  ~InstanceBlock();

  InstanceBlock(const InstanceBlock&) = delete;
  auto operator=(const InstanceBlock&) -> InstanceBlock& = delete;
  InstanceBlock(InstanceBlock&&) = delete;
  auto operator=(InstanceBlock&&) -> InstanceBlock& = delete;

  // Replaces the contents with `items`.
  template <typename T>
  void assign(const std::vector<T>& items) {
    assign_bytes(items.data(), items.size() * sizeof(T));
  }
  void assign_bytes(const void* data, std::size_t size);

  // How often the contents change; a hint for the backend's buffer usage.
  enum class Usage : std::uint8_t {
    Static,
    Dynamic
  };
  void set_usage(Usage usage) noexcept { m_usage = usage; }
  [[nodiscard]] auto usage() const noexcept -> Usage { return m_usage; }

  // Asks backends to size their buffer for at least `size` bytes up front and
  // to update it in place afterwards. Without a reservation a backend sizes
  // the buffer to the data on every upload.
  void reserve_bytes(std::size_t size) noexcept {
    if (size > m_reserved_bytes) {
      m_reserved_bytes = size;
    }
  }

  [[nodiscard]] auto data() const noexcept -> const std::byte* {
    return m_bytes.data();
  }
  [[nodiscard]] auto size_bytes() const noexcept -> std::size_t {
    return m_bytes.size();
  }
  [[nodiscard]] auto empty() const noexcept -> bool { return m_bytes.empty(); }
  [[nodiscard]] auto reserved_bytes() const noexcept -> std::size_t {
    return m_reserved_bytes;
  }
  [[nodiscard]] auto id() const noexcept -> std::uint64_t { return m_id; }
  [[nodiscard]] auto version() const noexcept -> std::uint64_t { return m_version; }
  [[nodiscard]] auto lifetime() const noexcept -> std::weak_ptr<const void> {
    return m_lifetime;
  }

private:
  std::vector<std::byte> m_bytes;
  std::size_t m_reserved_bytes = 0;
  Usage m_usage = Usage::Static;
  std::uint64_t m_id;
  std::uint64_t m_version = 0;
  std::shared_ptr<const int> m_lifetime;
};

// Reports instance blocks without handing over ownership, for prewarming.
using InstanceBlockVisitor = std::function<void(const InstanceBlock&)>;

} // namespace Render::GL
