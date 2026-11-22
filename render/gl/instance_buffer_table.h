#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "render/gl/buffer.h"

namespace Render::GL {

class InstanceBlock;

// The vertex buffers one backend keeps for the InstanceBlocks it draws. A
// block holds bytes only; the first time a table resolves it, the table makes
// a buffer on its own context, and it uploads again whenever the block's
// version moves. Buffers of destroyed blocks go at collect_retired().
class InstanceBufferTable {
public:
  InstanceBufferTable();
  ~InstanceBufferTable();

  InstanceBufferTable(const InstanceBufferTable&) = delete;
  auto operator=(const InstanceBufferTable&) -> InstanceBufferTable& = delete;
  InstanceBufferTable(InstanceBufferTable&&) = delete;
  auto operator=(InstanceBufferTable&&) -> InstanceBufferTable& = delete;

  // The buffer holding the block's current bytes, uploaded first if the block
  // changed. Null for a null block, or an empty block nothing was reserved
  // for.
  auto resolve(const InstanceBlock* block) -> Buffer*;

  // Creates the buffer, with its reserved storage and current contents, ahead
  // of the first draw. False if there is no context to create it on.
  auto prewarm(const InstanceBlock& block) -> bool;

  // Releases the buffers of blocks that no longer exist.
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
  // The last entry looked up. Draws of one mesh or block sit together in the
  // sorted queue, so most lookups hit it instead of hashing. Entry addresses
  // are stable in the map; anything that erases clears it.
  std::uint64_t m_last_id = 0;
  Entry* m_last = nullptr;
  std::vector<std::byte> m_scratch;
};

// Prewarms every block `visit_blocks` reports into `table`. `visit_blocks`
// takes an InstanceBlockVisitor, so a domain renderer can list its blocks
// without seeing GL.
template <typename VisitBlocks>
[[nodiscard]] auto prewarm_instance_blocks(InstanceBufferTable& table,
                                           VisitBlocks&& visit_blocks) -> bool {
  bool ready = true;
  visit_blocks(
      [&](const InstanceBlock& block) { ready = table.prewarm(block) && ready; });
  return ready;
}

} // namespace Render::GL
