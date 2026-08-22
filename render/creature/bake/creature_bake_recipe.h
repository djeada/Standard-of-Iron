#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "animation/clip_manifest.h"
#include "render/creature/schema/creature_runtime_manifest.h"

namespace Render::Creature {

struct BakeClipDescriptor {
  std::string_view name{};
  std::uint32_t frame_count{0U};
  float fps{0.0F};
  bool loops{false};
};

struct BakeSocketDescriptor {
  std::string_view name{};
  std::uint32_t anchor_bone{0U};
  QVector3D local_offset{};
};

using BakeClipFrameFn = void (*)(std::size_t clip_index,
                                 std::uint32_t frame_index,
                                 std::vector<QMatrix4x4>& out_palettes,
                                 std::vector<QMatrix4x4>* out_socket_transforms);

using BakeClipMarkersFn = void (*)(std::size_t clip_index,
                                   std::string_view clip_name,
                                   Animation::ClipMarkers& out);

struct CreatureBakeRecipe {

  const CreatureRuntimeManifest* runtime{nullptr};

  std::span<const BakeClipDescriptor> clips{};
  std::span<const BakeSocketDescriptor> sockets{};
  BakeClipFrameFn bake_clip_frame{nullptr};
  BakeClipMarkersFn clip_markers{nullptr};

  [[nodiscard]] auto complete() const noexcept -> bool {
    return runtime != nullptr && runtime->bind_palette != nullptr &&
           runtime->creature_spec != nullptr && runtime->topology != nullptr &&
           bake_clip_frame != nullptr;
  }
};

} // namespace Render::Creature
