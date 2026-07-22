#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "scatter_runtime.h"

namespace Render::Ground::Scatter {

template <typename Instance>
struct SpatialChunk {
  std::vector<Instance> instances;
  std::unique_ptr<Render::GL::Buffer> buffer;
  QVector3D center;
  float radius = 0.0F;
};

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
  std::size_t instance_count = 0;
  Params params{};
  bool instances_dirty = false;
  std::vector<Instance> visible_instances;
  std::vector<SpatialChunk<Instance>> spatial_chunks;
  std::uint64_t cached_visibility_version = 0;
  bool visibility_dirty = true;
  SyncStats last_sync_stats{};

  void reset_instances() {
    instances.clear();
    visible_instances.clear();
    spatial_chunks.clear();
    instance_count = 0;
    instances_dirty = false;
    cached_visibility_version = 0;
    visibility_dirty = true;
    last_sync_stats = {};
  }

  [[nodiscard]] auto is_gpu_ready() const -> bool {
    if (instances.empty() || (!visibility_dirty && visible_instances.empty())) {
      return true;
    }
    return !visibility_dirty &&
           std::all_of(spatial_chunks.begin(),
                       spatial_chunks.end(),
                       [](const auto& chunk) { return chunk.buffer != nullptr; });
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
                         PositionAccessor position_accessor,
                         const Game::Map::VisibilityService::Snapshot* snapshot)
    -> std::uint32_t {
  state.last_sync_stats = {};
  if (state.instances.empty()) {
    state.visible_instances.clear();
    state.spatial_chunks.clear();
    state.instance_count = 0;
    state.cached_visibility_version = 0;
    state.visibility_dirty = false;
    return 0;
  }

  const std::uint64_t version = snapshot != nullptr ? snapshot->version : 0;
  const bool rebuild = state.instances_dirty || state.visibility_dirty ||
                       version != state.cached_visibility_version ||
                       (state.instance_count > 0 && state.spatial_chunks.empty());
  if (!rebuild) {
    return static_cast<std::uint32_t>(state.instance_count);
  }

  ++state.last_sync_stats.visibility_rebuilds;
  state.visible_instances.clear();
  state.visible_instances.reserve(state.instances.size());
  state.spatial_chunks.clear();

  constexpr float k_chunk_world_size = 24.0F;
  constexpr float k_bounds_padding = 8.0F;
  struct Bounds {
    QVector3D min{std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
    QVector3D max{std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest()};
  };
  std::vector<Bounds> bounds;
  std::unordered_map<std::uint64_t, std::size_t> chunk_indices;
  chunk_indices.reserve(state.instances.size() / 16U + 1U);

  for (const auto& instance : state.instances) {
    const auto position = position_accessor(instance);
    if (snapshot != nullptr &&
        Game::Map::classify_world_visibility(*snapshot, position.x(), position.z()) !=
            Game::Map::RenderVisibilityState::Visible) {
      continue;
    }
    state.visible_instances.push_back(instance);
    const auto chunk_x = static_cast<std::int32_t>(
        std::floor(position.x() / k_chunk_world_size));
    const auto chunk_z = static_cast<std::int32_t>(
        std::floor(position.z() / k_chunk_world_size));
    const std::uint64_t key =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunk_x)) << 32U) |
        static_cast<std::uint32_t>(chunk_z);
    auto [it, inserted] = chunk_indices.emplace(key, state.spatial_chunks.size());
    if (inserted) {
      state.spatial_chunks.emplace_back();
      bounds.emplace_back();
    }
    const std::size_t index = it->second;
    state.spatial_chunks[index].instances.push_back(instance);
    auto& chunk_bounds = bounds[index];
    chunk_bounds.min.setX(std::min(chunk_bounds.min.x(), position.x()));
    chunk_bounds.min.setY(std::min(chunk_bounds.min.y(), position.y()));
    chunk_bounds.min.setZ(std::min(chunk_bounds.min.z(), position.z()));
    chunk_bounds.max.setX(std::max(chunk_bounds.max.x(), position.x()));
    chunk_bounds.max.setY(std::max(chunk_bounds.max.y(), position.y()));
    chunk_bounds.max.setZ(std::max(chunk_bounds.max.z(), position.z()));
  }

  for (std::size_t index = 0; index < state.spatial_chunks.size(); ++index) {
    auto& chunk = state.spatial_chunks[index];
    chunk.center = (bounds[index].min + bounds[index].max) * 0.5F;
    chunk.radius = (bounds[index].max - chunk.center).length() + k_bounds_padding;
    chunk.buffer =
        std::make_unique<Render::GL::Buffer>(Render::GL::Buffer::Type::Vertex);
    chunk.buffer->set_data(chunk.instances, Render::GL::Buffer::Usage::Static);
    ++state.last_sync_stats.buffer_uploads;
  }

  state.instance_count = state.visible_instances.size();
  state.instances_dirty = false;
  state.visibility_dirty = false;
  state.cached_visibility_version = version;
  return static_cast<std::uint32_t>(state.instance_count);
}

template <typename Instance, typename Params, typename PositionAccessor>
auto sync_filtered_state(FilteredRendererState<Instance, Params>& state,
                         PositionAccessor position_accessor) -> std::uint32_t {
  auto& visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility =
      visibility_filter_enabled_for_current_thread() && visibility.is_initialized();
  const auto snapshot = use_visibility ? visibility.snapshot_ptr() : nullptr;
  return sync_filtered_state(state, position_accessor, snapshot.get());
}

} // namespace Render::Ground::Scatter
