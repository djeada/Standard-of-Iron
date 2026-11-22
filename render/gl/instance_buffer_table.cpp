#include "render/gl/instance_buffer_table.h"

#include <QOpenGLContext>

#include <algorithm>

#include "render/geom/instance_block.h"

namespace Render::GL {

InstanceBufferTable::InstanceBufferTable() = default;
InstanceBufferTable::~InstanceBufferTable() = default;

auto InstanceBufferTable::entry_for(const InstanceBlock& block) -> Entry& {
  if (m_last == nullptr || m_last_id != block.id()) {
    m_last = &m_entries[block.id()];
    m_last_id = block.id();
  }
  Entry& entry = *m_last;
  if (entry.buffer == nullptr) {
    entry.lifetime = block.lifetime();
    entry.buffer = std::make_unique<Buffer>(
        block.target() == InstanceBlock::Target::Uniform ? Buffer::Type::Uniform
                                                         : Buffer::Type::Vertex);
  }
  return entry;
}

auto InstanceBufferTable::resolve(const InstanceBlock* block) -> Buffer* {
  if (block == nullptr || (block->empty() && block->reserved_bytes() == 0)) {
    return nullptr;
  }
  Entry& entry = entry_for(*block);
  if (!entry.uploaded || entry.uploaded_version != block->version()) {
    const std::byte* bytes = block->data();
    if (bytes == nullptr && !block->empty()) {
      m_scratch.resize(block->size_bytes());
      block->write_to(m_scratch);
      bytes = m_scratch.data();
    }
    if (block->reserved_bytes() > 0) {
      // Reserved: size once, then update in place.
      entry.buffer->reserve(std::max(block->reserved_bytes(), block->size_bytes()));
      entry.buffer->update_sub_data(bytes, block->size_bytes());
    } else {
      entry.buffer->set_data(bytes,
                             block->size_bytes(),
                             block->usage() == InstanceBlock::Usage::Dynamic
                                 ? Buffer::Usage::Dynamic
                                 : Buffer::Usage::Static);
    }
    entry.uploaded_version = block->version();
    entry.uploaded = true;
  }
  return entry.buffer.get();
}

auto InstanceBufferTable::prewarm(const InstanceBlock& block) -> bool {
  if (QOpenGLContext::currentContext() == nullptr) {
    return false;
  }
  if (block.empty() && block.reserved_bytes() == 0) {
    return true;
  }
  const Buffer* buffer = resolve(&block);
  return buffer != nullptr && buffer->id() != 0U;
}

void InstanceBufferTable::collect_retired() {
  m_last = nullptr;
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (it->second.lifetime.expired()) {
      it = m_entries.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace Render::GL
