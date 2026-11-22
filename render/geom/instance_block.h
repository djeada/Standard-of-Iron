#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace Render::GL {

// Buffer data a draw command points at: per-instance data (grass blades,
// stones, fog puffs) or uniform data (baked bone palettes). It holds bytes, or a
// writer that produces them, and no GL object. A domain renderer owns and fills it;
// each backend keeps its own vertex buffer for it in an InstanceBufferTable, keyed by
// id(), uploads the bytes again whenever version() moves, and drops the buffer once the
// block is destroyed.
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

  // Replaces the contents with `size` bytes that `writer` produces when a
  // backend uploads them, so large derived data (a bone palette atlas) is not
  // kept twice on the CPU. The writer must stay valid while the block lives.
  using Writer = std::function<void(std::span<std::byte>)>;
  void assign_generated(std::size_t size, Writer writer);

  // Copies the contents into `destination`, which holds size_bytes().
  void write_to(std::span<std::byte> destination) const;

  // Which buffer target backends create for it.
  enum class Target : std::uint8_t {
    Vertex,
    Uniform
  };
  void set_target(Target target) noexcept { m_target = target; }
  [[nodiscard]] auto target() const noexcept -> Target { return m_target; }

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

  // The bytes, or null for generated contents (use write_to()).
  [[nodiscard]] auto data() const noexcept -> const std::byte* {
    return m_writer ? nullptr : m_bytes.data();
  }
  [[nodiscard]] auto size_bytes() const noexcept -> std::size_t {
    return m_writer ? m_generated_bytes : m_bytes.size();
  }
  [[nodiscard]] auto empty() const noexcept -> bool { return size_bytes() == 0; }
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
  Writer m_writer;
  std::size_t m_generated_bytes = 0;
  std::size_t m_reserved_bytes = 0;
  Target m_target = Target::Vertex;
  Usage m_usage = Usage::Static;
  std::uint64_t m_id;
  std::uint64_t m_version = 0;
  std::shared_ptr<const int> m_lifetime;
};

// Reports instance blocks without handing over ownership, for prewarming.
using InstanceBlockVisitor = std::function<void(const InstanceBlock&)>;

} // namespace Render::GL
