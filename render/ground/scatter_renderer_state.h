#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "scatter_runtime.h"

namespace Render::Ground::Scatter {

template <typename Instance, typename Params>
struct DirectRendererState {
  std::vector<Instance> instances;
  std::unique_ptr<Render::GL::Buffer> instance_buffer;
  std::size_t instance_count = 0;
  Params params{};
  bool instances_dirty = false;
  SyncStats last_sync_stats{};

  void reset_instances() {
    instances.clear();
    instance_count = 0;
    instances_dirty = false;
    last_sync_stats = {};
  }

  [[nodiscard]] auto is_gpu_ready() const -> bool {
    return is_direct_gpu_ready(instances, instance_buffer);
  }
};

template <typename Instance, typename Params>
struct FilteredRendererState {
  std::vector<Instance> instances;
  std::unique_ptr<Render::GL::Buffer> instance_buffer;
  std::size_t instance_count = 0;
  Params params{};
  bool instances_dirty = false;
  std::vector<Instance> visible_instances;
  std::uint64_t cached_visibility_version = 0;
  bool visibility_dirty = true;
  SyncStats last_sync_stats{};

  void reset_instances() {
    instances.clear();
    visible_instances.clear();
    instance_count = 0;
    instances_dirty = false;
    cached_visibility_version = 0;
    visibility_dirty = true;
    last_sync_stats = {};
  }

  [[nodiscard]] auto is_gpu_ready() const -> bool {
    return is_filtered_gpu_ready(
        instances, visible_instances, instance_buffer, visibility_dirty);
  }
};

template <typename Instance, typename Params>
void sync_direct_state(DirectRendererState<Instance, Params>& state) {
  state.last_sync_stats = {};
  state.instance_count = sync_direct_instances(state.instances,
                                               state.instance_buffer,
                                               state.instances_dirty,
                                               &state.last_sync_stats);
}

template <typename Instance, typename Params, typename PositionAccessor>
auto sync_filtered_state(FilteredRendererState<Instance, Params>& state,
                         PositionAccessor position_accessor) -> std::uint32_t {
  state.last_sync_stats = {};
  state.instance_count = sync_filtered_instances(state.instances,
                                                 state.visible_instances,
                                                 state.instance_buffer,
                                                 state.cached_visibility_version,
                                                 state.visibility_dirty,
                                                 position_accessor,
                                                 &state.last_sync_stats);
  return static_cast<std::uint32_t>(state.instance_count);
}

} // namespace Render::Ground::Scatter
