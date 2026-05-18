#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "../../game/map/render_visibility_rules.h"
#include "../../game/map/visibility_service.h"
#include "../gl/buffer.h"

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

struct SyncStats {
  std::uint32_t visibility_rebuilds = 0;
  std::uint32_t buffer_uploads = 0;
  std::uint32_t buffer_resets = 0;

  [[nodiscard]] auto did_upload_or_rebuild() const noexcept -> bool {
    return visibility_rebuilds != 0U || buffer_uploads != 0U || buffer_resets != 0U;
  }

  auto operator+=(const SyncStats& other) noexcept -> SyncStats& {
    visibility_rebuilds += other.visibility_rebuilds;
    buffer_uploads += other.buffer_uploads;
    buffer_resets += other.buffer_resets;
    return *this;
  }
};

[[nodiscard]] inline auto direct_needs_buffer_upload(
    bool instances_empty, bool has_buffer, bool instances_dirty) noexcept -> bool {
  return !instances_empty && (instances_dirty || !has_buffer);
}

[[nodiscard]] inline auto filtered_needs_visibility_rebuild(
    bool instances_empty,
    bool visible_instances_empty,
    bool has_buffer,
    bool visibility_dirty,
    std::uint64_t current_visibility_version,
    std::uint64_t cached_visibility_version) noexcept -> bool {
  if (instances_empty) {
    return false;
  }
  return visibility_dirty ||
         (current_visibility_version != cached_visibility_version) ||
         (!visible_instances_empty && !has_buffer);
}

template <typename Instance, typename PositionAccessor>
auto collect_visible_instances(const std::vector<Instance>& instances,
                               const Game::Map::VisibilityService::Snapshot& snapshot,
                               PositionAccessor position_accessor)
    -> std::vector<Instance> {
  if (!snapshot.initialized) {
    return instances;
  }

  std::vector<Instance> visible_instances;
  visible_instances.reserve(instances.size());
  for (const auto& instance : instances) {
    const auto position = position_accessor(instance);
    if (Game::Map::classify_world_visibility(snapshot, position.x(), position.z()) ==
        Game::Map::RenderVisibilityState::Visible) {
      visible_instances.push_back(instance);
    }
  }
  return visible_instances;
}

template <typename Instance, typename PositionAccessor>
auto sync_filtered_instances(const std::vector<Instance>& instances,
                             std::vector<Instance>& visible_instances,
                             std::unique_ptr<Render::GL::Buffer>& instance_buffer,
                             std::uint64_t& cached_visibility_version,
                             bool& visibility_dirty,
                             PositionAccessor position_accessor,
                             SyncStats* stats = nullptr) -> std::uint32_t {
  if (instances.empty()) {
    if (instance_buffer && stats != nullptr) {
      ++stats->buffer_resets;
    }
    visible_instances.clear();
    instance_buffer.reset();
    cached_visibility_version = 0;
    visibility_dirty = false;
    return 0;
  }

  auto& visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility =
      visibility_filter_enabled_for_current_thread() && visibility.is_initialized();
  auto visibility_snapshot = use_visibility ? visibility.snapshot_ptr() : nullptr;
  const std::uint64_t current_version =
      visibility_snapshot != nullptr ? visibility_snapshot->version : 0;
  const bool needs_visibility_update =
      filtered_needs_visibility_rebuild(instances.empty(),
                                        visible_instances.empty(),
                                        instance_buffer != nullptr,
                                        visibility_dirty,
                                        current_version,
                                        cached_visibility_version);

  if (needs_visibility_update) {
    if (stats != nullptr) {
      ++stats->visibility_rebuilds;
    }
    if (visibility_snapshot != nullptr) {
      visible_instances =
          collect_visible_instances(instances, *visibility_snapshot, position_accessor);
    } else {
      visible_instances = instances;
    }

    cached_visibility_version = current_version;
    visibility_dirty = false;

    if (!visible_instances.empty()) {
      if (!instance_buffer) {
        instance_buffer =
            std::make_unique<Render::GL::Buffer>(Render::GL::Buffer::Type::Vertex);
      }
      instance_buffer->set_data(visible_instances, Render::GL::Buffer::Usage::Static);
      if (stats != nullptr) {
        ++stats->buffer_uploads;
      }
    } else {
      if (instance_buffer && stats != nullptr) {
        ++stats->buffer_resets;
      }
      instance_buffer.reset();
    }
  }

  return static_cast<std::uint32_t>(visible_instances.size());
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
auto is_filtered_gpu_ready(const std::vector<Instance>& instances,
                           const std::vector<Instance>& visible_instances,
                           const std::unique_ptr<Render::GL::Buffer>& buffer,
                           bool visibility_dirty) -> bool {
  if (instances.empty()) {
    return true;
  }
  if (!visibility_dirty && visible_instances.empty()) {
    return true;
  }
  return buffer != nullptr && !visibility_dirty;
}

template <typename Instance>
auto is_direct_gpu_ready(const std::vector<Instance>& instances,
                         const std::unique_ptr<Render::GL::Buffer>& buffer) -> bool {
  return buffer != nullptr || instances.empty();
}

} // namespace Render::Ground::Scatter
