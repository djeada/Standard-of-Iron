#include "render/geom/instance_block.h"

#include <atomic>
#include <cstring>

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
  m_bytes.resize(size);
  if (size > 0) {
    std::memcpy(m_bytes.data(), data, size);
  }
  ++m_version;
}

} // namespace Render::GL
