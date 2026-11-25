#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace Render::GL {

class InstanceBlock {
public:
  InstanceBlock();
  ~InstanceBlock();

  InstanceBlock(const InstanceBlock&) = delete;
  auto operator=(const InstanceBlock&) -> InstanceBlock& = delete;
  InstanceBlock(InstanceBlock&&) = delete;
  auto operator=(InstanceBlock&&) -> InstanceBlock& = delete;

  template <typename T>
  void assign(const std::vector<T>& items) {
    assign_bytes(items.data(), items.size() * sizeof(T));
  }
  void assign_bytes(const void* data, std::size_t size);

  using Writer = std::function<void(std::span<std::byte>)>;
  void assign_generated(std::size_t size, Writer writer);

  void write_to(std::span<std::byte> destination) const;

  enum class Target : std::uint8_t {
    Vertex,
    Uniform
  };
  void set_target(Target target) noexcept { m_target = target; }
  [[nodiscard]] auto target() const noexcept -> Target { return m_target; }

  enum class Usage : std::uint8_t {
    Static,
    Dynamic
  };
  void set_usage(Usage usage) noexcept { m_usage = usage; }
  [[nodiscard]] auto usage() const noexcept -> Usage { return m_usage; }

  void reserve_bytes(std::size_t size) noexcept {
    if (size > m_reserved_bytes) {
      m_reserved_bytes = size;
    }
  }

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

using InstanceBlockVisitor = std::function<void(const InstanceBlock&)>;

} // namespace Render::GL
