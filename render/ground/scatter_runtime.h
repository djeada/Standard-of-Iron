#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "render/frame/terrain_scene_types.h"
#include "render/gl/buffer.h"

namespace Render::Ground::Scatter {

inline auto visibility_filter_enabled_ref() -> bool& {
  static thread_local bool enabled = true;
  return enabled;
}

inline void set_visibility_filter_enabled_for_current_thread(bool enabled) {
  visibility_filter_enabled_ref() = enabled;
}

[[nodiscard]] inline auto visibility_filter_enabled_for_current_thread() -> bool {
  return visibility_filter_enabled_ref();
}

[[nodiscard]] inline auto direct_needs_buffer_upload(
    bool instances_empty, bool has_buffer, bool instances_dirty) noexcept -> bool {
  return !instances_empty && (instances_dirty || !has_buffer);
}

template <typename Instance>
auto sync_direct_instances(const std::vector<Instance>& instances,
                           std::unique_ptr<Render::GL::Buffer>& instance_buffer,
                           bool& instances_dirty,
                           SyncStats* stats = nullptr) -> std::uint32_t {
  if (instances.empty()) {
    if (instance_buffer && stats != nullptr) {
      ++stats->buffer_resets;
    }
    instance_buffer.reset();
    instances_dirty = false;
    return 0;
  }

  const bool needs_upload = direct_needs_buffer_upload(
      instances.empty(), instance_buffer != nullptr, instances_dirty);
  if (!instance_buffer) {
    instance_buffer =
        std::make_unique<Render::GL::Buffer>(Render::GL::Buffer::Type::Vertex);
  }
  if (needs_upload && instance_buffer) {
    instance_buffer->set_data(instances, Render::GL::Buffer::Usage::Static);
    instances_dirty = false;
    if (stats != nullptr) {
      ++stats->buffer_uploads;
    }
  }

  return static_cast<std::uint32_t>(instances.size());
}

template <typename Instance>
auto is_direct_gpu_ready(const std::vector<Instance>& instances,
                         const std::unique_ptr<Render::GL::Buffer>& buffer) -> bool {
  return buffer != nullptr || instances.empty();
}

} // namespace Render::Ground::Scatter
