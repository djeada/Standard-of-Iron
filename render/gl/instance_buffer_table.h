#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "render/gl/buffer.h"

namespace Render::GL {

class InstanceBlock;

class InstanceBufferTable {
public:
  InstanceBufferTable();
  ~InstanceBufferTable();

  InstanceBufferTable(const InstanceBufferTable&) = delete;
  auto operator=(const InstanceBufferTable&) -> InstanceBufferTable& = delete;
  InstanceBufferTable(InstanceBufferTable&&) = delete;
  auto operator=(InstanceBufferTable&&) -> InstanceBufferTable& = delete;

  auto resolve(const InstanceBlock* block) -> Buffer*;

  auto prewarm(const InstanceBlock& block) -> bool;

  void collect_retired();

  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_entries.size(); }

private:
  struct Entry {
    std::weak_ptr<const void> lifetime;
    std::unique_ptr<Buffer> buffer;
    std::uint64_t uploaded_version = 0;
    bool uploaded = false;
  };

  auto entry_for(const InstanceBlock& block) -> Entry&;

  std::unordered_map<std::uint64_t, Entry> m_entries;

  std::uint64_t m_last_id = 0;
  Entry* m_last = nullptr;
  std::vector<std::byte> m_scratch;
};

template <typename VisitBlocks>
[[nodiscard]] auto prewarm_instance_blocks(InstanceBufferTable& table,
                                           VisitBlocks&& visit_blocks) -> bool {
  bool ready = true;
  visit_blocks(
      [&](const InstanceBlock& block) { ready = table.prewarm(block) && ready; });
  return ready;
}

} // namespace Render::GL
