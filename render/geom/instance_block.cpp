#include "render/geom/instance_block.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>

namespace Render::GL {

namespace {

auto next_instance_block_id() -> std::uint64_t {
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

InstanceBlock::InstanceBlock()
    : m_id(next_instance_block_id())
    , m_lifetime(std::make_shared<const int>(0)) {
}

InstanceBlock::~InstanceBlock() = default;

void InstanceBlock::assign_bytes(const void* data, std::size_t size) {
  m_writer = nullptr;
  m_generated_bytes = 0;
  m_bytes.resize(size);
  if (size > 0) {
    std::memcpy(m_bytes.data(), data, size);
  }
  ++m_version;
}

void InstanceBlock::assign_generated(std::size_t size, Writer writer) {
  m_bytes.clear();
  m_bytes.shrink_to_fit();
  m_generated_bytes = writer ? size : 0;
  m_writer = std::move(writer);
  ++m_version;
}

void InstanceBlock::write_to(std::span<std::byte> destination) const {
  const std::size_t size = std::min(destination.size(), size_bytes());
  if (size == 0) {
    return;
  }
  if (m_writer) {
    m_writer(destination.first(size));
    return;
  }
  std::memcpy(destination.data(), m_bytes.data(), size);
}

} // namespace Render::GL
