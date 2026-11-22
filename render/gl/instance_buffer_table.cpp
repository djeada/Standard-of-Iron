#include "render/gl/instance_buffer_table.h"

#include <QOpenGLContext>

#include <algorithm>

#include "render/geom/instance_block.h"

namespace Render::GL {

InstanceBufferTable::InstanceBufferTable() = default;
InstanceBufferTable::~InstanceBufferTable() = default;

auto InstanceBufferTable::entry_for(const InstanceBlock& block) -> Entry& {
  Entry& entry = m_entries[block.id()];
  if (entry.buffer == nullptr) {
    entry.lifetime = block.lifetime();
    entry.buffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
  }
  return entry;
}

auto InstanceBufferTable::resolve(const InstanceBlock* block) -> Buffer* {
  if (block == nullptr || (block->empty() && block->reserved_bytes() == 0)) {
    return nullptr;
  }
  Entry& entry = entry_for(*block);
  if (!entry.uploaded || entry.uploaded_version != block->version()) {
    if (block->reserved_bytes() > 0) {
      // Reserved: size once, then update in place.
      entry.buffer->reserve(std::max(block->reserved_bytes(), block->size_bytes()));
      entry.buffer->update_sub_data(block->data(), block->size_bytes());
    } else {
      entry.buffer->set_data(block->data(),
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
  if (block.reserved_bytes() == 0) {
    return true;
  }
  Entry& entry = entry_for(block);
  entry.buffer->reserve(block.reserved_bytes());
  return entry.buffer->id() != 0U;
}

void InstanceBufferTable::collect_retired() {
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (it->second.lifetime.expired()) {
      it = m_entries.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace Render::GL
