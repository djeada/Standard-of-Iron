#pragma once

#include "../../game/map/visibility_service.h"
#include "../gl/buffer.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::Ground::Scatter {

template <typename Instance, typename PositionAccessor>
auto collect_visible_instances(
    const std::vector<Instance> &instances,
    const Game::Map::VisibilityService::Snapshot &snapshot,
    PositionAccessor position_accessor) -> std::vector<Instance> {
  if (!snapshot.initialized) {
    return instances;
  }

  std::vector<Instance> visible_instances;
  visible_instances.reserve(instances.size());
  for (const auto &instance : instances) {
    const auto position = position_accessor(instance);
    if (snapshot.isVisibleWorld(position.x(), position.z())) {
      visible_instances.push_back(instance);
    }
  }
  return visible_instances;
}

template <typename Instance, typename PositionAccessor>
auto sync_filtered_instances(
    const std::vector<Instance> &instances,
    std::vector<Instance> &visible_instances,
    std::unique_ptr<Render::GL::Buffer> &instance_buffer,
    std::uint64_t &cached_visibility_version, bool &visibility_dirty,
    PositionAccessor position_accessor) -> std::uint32_t {
  if (instances.empty()) {
    visible_instances.clear();
    instance_buffer.reset();
    cached_visibility_version = 0;
    visibility_dirty = false;
    return 0;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.is_initialized();
  const std::uint64_t current_version =
      use_visibility ? visibility.version() : 0;
  const bool needs_visibility_update =
      visibility_dirty || (current_version != cached_visibility_version) ||
      (!visible_instances.empty() && !instance_buffer);

  if (needs_visibility_update) {
    if (use_visibility) {
      visible_instances = collect_visible_instances(
          instances, visibility.snapshot(), position_accessor);
    } else {
      visible_instances = instances;
    }

    cached_visibility_version = current_version;
    visibility_dirty = false;

    if (!visible_instances.empty()) {
      if (!instance_buffer) {
        instance_buffer = std::make_unique<Render::GL::Buffer>(
            Render::GL::Buffer::Type::Vertex);
      }
      instance_buffer->set_data(visible_instances,
                                Render::GL::Buffer::Usage::Static);
    } else {
      instance_buffer.reset();
    }
  }

  return static_cast<std::uint32_t>(visible_instances.size());
}

template <typename Instance>
auto sync_direct_instances(const std::vector<Instance> &instances,
                           std::unique_ptr<Render::GL::Buffer> &instance_buffer,
                           bool &instances_dirty) -> std::uint32_t {
  if (instances.empty()) {
    instance_buffer.reset();
    instances_dirty = false;
    return 0;
  }

  const bool needs_upload = instances_dirty || !instance_buffer;
  if (!instance_buffer) {
    instance_buffer =
        std::make_unique<Render::GL::Buffer>(Render::GL::Buffer::Type::Vertex);
  }
  if (needs_upload && instance_buffer) {
    instance_buffer->set_data(instances, Render::GL::Buffer::Usage::Static);
    instances_dirty = false;
  }

  return static_cast<std::uint32_t>(instances.size());
}

template <typename Instance>
auto is_filtered_gpu_ready(const std::vector<Instance> &instances,
                           const std::vector<Instance> &visible_instances,
                           const std::unique_ptr<Render::GL::Buffer> &buffer,
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
auto is_direct_gpu_ready(const std::vector<Instance> &instances,
                         const std::unique_ptr<Render::GL::Buffer> &buffer)
    -> bool {
  return buffer != nullptr || instances.empty();
}

} // namespace Render::Ground::Scatter
