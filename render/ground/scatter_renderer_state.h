#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "scatter_runtime.h"

namespace Render::Ground::Scatter {

inline constexpr float k_chunk_world_size = 24.0F;
inline constexpr float k_chunk_bounds_padding = 8.0F;

template <typename Instance>
struct SpatialChunk {
  std::size_t first = 0;
  std::size_t count = 0;
  std::vector<std::uint8_t> accepted;
  std::unique_ptr<Render::GL::Buffer> buffer;
  std::size_t visible_count = 0;
  bool all_accepted = false;
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

  bool track_visible_instances = false;
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
    if (instances.empty()) {
      return true;
    }
    if (visibility_dirty) {
      return false;
    }
    return std::all_of(
        spatial_chunks.begin(), spatial_chunks.end(), [](const auto& chunk) {
          return chunk.visible_count == 0 || chunk.buffer != nullptr;
        });
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

enum class ScatterMemoryMode : std::uint8_t {
  VisibleOnly,
  Remembered
};

template <typename Instance, typename PositionAccessor>
void rebuild_spatial_partition(std::vector<Instance>& instances,
                               std::vector<SpatialChunk<Instance>>& chunks,
                               PositionAccessor position_accessor) {
  chunks.clear();
  if (instances.empty()) {
    return;
  }

  std::unordered_map<std::uint64_t, std::size_t> chunk_indices;
  chunk_indices.reserve(instances.size() / 16U + 1U);
  std::vector<std::size_t> chunk_of_instance(instances.size(), 0U);
  std::vector<std::size_t> counts;

  for (std::size_t index = 0; index < instances.size(); ++index) {
    const auto position = position_accessor(instances[index]);
    const auto chunk_x =
        static_cast<std::int32_t>(std::floor(position.x() / k_chunk_world_size));
    const auto chunk_z =
        static_cast<std::int32_t>(std::floor(position.z() / k_chunk_world_size));
    const std::uint64_t key =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunk_x)) << 32U) |
        static_cast<std::uint32_t>(chunk_z);
    auto [it, inserted] = chunk_indices.emplace(key, chunks.size());
    if (inserted) {
      chunks.emplace_back();
      counts.push_back(0U);
    }
    chunk_of_instance[index] = it->second;
    ++counts[it->second];
  }

  std::size_t offset = 0;
  std::vector<std::size_t> cursor(chunks.size(), 0U);
  for (std::size_t index = 0; index < chunks.size(); ++index) {
    chunks[index].first = offset;
    chunks[index].count = counts[index];
    cursor[index] = offset;
    offset += counts[index];
  }

  std::vector<Instance> reordered(instances.size());
  for (std::size_t index = 0; index < instances.size(); ++index) {
    reordered[cursor[chunk_of_instance[index]]++] = instances[index];
  }
  instances = std::move(reordered);

  for (auto& chunk : chunks) {
    QVector3D min{std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
    QVector3D max{std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest(),
                  std::numeric_limits<float>::lowest()};
    for (std::size_t i = 0; i < chunk.count; ++i) {
      const auto position = position_accessor(instances[chunk.first + i]);
      min.setX(std::min(min.x(), position.x()));
      min.setY(std::min(min.y(), position.y()));
      min.setZ(std::min(min.z(), position.z()));
      max.setX(std::max(max.x(), position.x()));
      max.setY(std::max(max.y(), position.y()));
      max.setZ(std::max(max.z(), position.z()));
    }
    chunk.center = (min + max) * 0.5F;
    chunk.radius = (max - chunk.center).length() + k_chunk_bounds_padding;
    chunk.accepted.assign(chunk.count, 0U);
    chunk.visible_count = 0;
    chunk.all_accepted = false;
    chunk.buffer.reset();
  }
}

template <typename Instance, typename PositionAccessor, typename Accepts>
auto refresh_chunk_acceptance(const std::vector<Instance>& instances,
                              SpatialChunk<Instance>& chunk,
                              PositionAccessor position_accessor,
                              Accepts accepts,
                              std::vector<std::uint8_t>& flags,
                              std::vector<Instance>& packed) -> bool {
  flags.assign(chunk.count, 0U);
  packed.clear();

  bool changed = chunk.accepted.size() != chunk.count;
  for (std::size_t i = 0; i < chunk.count; ++i) {
    const auto& instance = instances[chunk.first + i];
    const auto position = position_accessor(instance);
    const bool is_accepted = accepts(position.x(), position.z());
    flags[i] = is_accepted ? std::uint8_t{1} : std::uint8_t{0};
    if (is_accepted) {
      packed.push_back(instance);
    }
    if (!changed && flags[i] != chunk.accepted[i]) {
      changed = true;
    }
  }

  if (!changed) {
    return false;
  }

  chunk.accepted.assign(flags.begin(), flags.end());
  chunk.visible_count = packed.size();
  chunk.all_accepted = chunk.visible_count == chunk.count;
  return true;
}

template <typename Instance, typename Params, typename PositionAccessor>
auto sync_filtered_state(FilteredRendererState<Instance, Params>& state,
                         PositionAccessor position_accessor,
                         const Game::Map::VisibilityService::Snapshot* snapshot,
                         ScatterMemoryMode memory_mode = ScatterMemoryMode::VisibleOnly)
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
  const bool structure_dirty = state.instances_dirty || state.spatial_chunks.empty();
  const bool visibility_changed =
      state.visibility_dirty || version != state.cached_visibility_version;
  if (!structure_dirty && !visibility_changed) {
    return static_cast<std::uint32_t>(state.instance_count);
  }

  ++state.last_sync_stats.visibility_rebuilds;
  if (structure_dirty) {
    rebuild_spatial_partition(state.instances, state.spatial_chunks, position_accessor);
  }

  const bool remembered = memory_mode == ScatterMemoryMode::Remembered;
  const auto accepts = [snapshot, remembered](float world_x, float world_z) -> bool {
    if (snapshot == nullptr) {
      return true;
    }
    const auto visibility =
        Game::Map::classify_world_visibility(*snapshot, world_x, world_z);
    return remembered ? visibility != Game::Map::RenderVisibilityState::Hidden
                      : visibility == Game::Map::RenderVisibilityState::Visible;
  };

  std::vector<std::uint8_t> flags;
  std::vector<Instance> packed;
  std::size_t total_visible = 0;
  bool any_chunk_changed = false;

  for (auto& chunk : state.spatial_chunks) {

    if (remembered && chunk.all_accepted && chunk.buffer != nullptr) {
      total_visible += chunk.visible_count;
      continue;
    }

    ++state.last_sync_stats.chunks_rescanned;
    const bool changed = refresh_chunk_acceptance(
        state.instances, chunk, position_accessor, accepts, flags, packed);
    total_visible += chunk.visible_count;
    if (!changed && chunk.buffer != nullptr) {
      continue;
    }
    any_chunk_changed = true;

    if (chunk.visible_count == 0) {
      if (chunk.buffer != nullptr) {
        chunk.buffer.reset();
        ++state.last_sync_stats.buffer_resets;
      }
      continue;
    }
    if (!chunk.buffer) {
      chunk.buffer =
          std::make_unique<Render::GL::Buffer>(Render::GL::Buffer::Type::Vertex);
    }
    chunk.buffer->set_data(packed, Render::GL::Buffer::Usage::Static);
    ++state.last_sync_stats.buffer_uploads;
  }

  if (state.track_visible_instances && (any_chunk_changed || structure_dirty)) {
    state.visible_instances.clear();
    state.visible_instances.reserve(total_visible);
    for (const auto& chunk : state.spatial_chunks) {
      for (std::size_t i = 0; i < chunk.count; ++i) {
        if (chunk.accepted[i] != 0U) {
          state.visible_instances.push_back(state.instances[chunk.first + i]);
        }
      }
    }
  }

  state.instance_count = total_visible;
  state.instances_dirty = false;
  state.visibility_dirty = false;
  state.cached_visibility_version = version;
  return static_cast<std::uint32_t>(state.instance_count);
}

} // namespace Render::Ground::Scatter
