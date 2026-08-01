#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "animation/clip_manifest.h"
#include "part_graph.h"
#include "quadruped/mesh_graph.h"
#include "spec.h"

namespace Render::Creature {

struct BakeClipDescriptor {
  std::string_view name{};
  std::uint32_t frame_count{0U};
  float fps{0.0F};
  bool loops{false};
};

// An attachment point carried through to the baked animation table, so that
// equipment can be positioned from prebaked data instead of being re-derived
// from a pose at draw time.
struct BakeSocketDescriptor {
  std::string_view name{};
  std::uint32_t anchor_bone{0U};
  QVector3D local_offset{};
};

using BindPaletteProviderFn = std::span<const QMatrix4x4> (*)() noexcept;
using CreatureSpecProviderFn = const CreatureSpec& (*)() noexcept;

// Produces one frame of a clip. Palettes are always required; a species with no
// sockets is passed a null socket sink. Palettes and sockets come from the same
// call because they are derived from the same pose - computing them separately
// would both double the work and let them drift apart.
using BakeClipFrameFn = void (*)(std::size_t clip_index,
                                 std::uint32_t frame_index,
                                 std::vector<QMatrix4x4>& out_palettes,
                                 std::vector<QMatrix4x4>* out_socket_transforms);

// Fills in authored timing markers for a clip. Species that leave this null get
// the generic markers looked up from the clip name.
using BakeClipMarkersFn = void (*)(std::size_t clip_index,
                                   std::string_view clip_name,
                                   Animation::ClipMarkers& out);

struct WholeMeshLodManifest {
  std::string_view primitive_name{};
  BoneIndex anchor_bone{k_invalid_bone};
  MeshSkinning mesh_skinning{MeshSkinning::Rigid};
  std::uint8_t color_role{0U};
  int material_id{0};
  std::uint8_t lod_mask{k_lod_all};
  std::span<const Quadruped::MeshNode> mesh_nodes{};
  std::span<const std::string_view> excluded_node_name_prefixes{};
  std::span<const PrimitiveInstance> overlay_primitives{};
};

struct SpeciesManifest {
  std::string_view species_name{};
  std::uint32_t species_id{0U};
  std::string_view bpat_file_name{};
  std::string_view minimal_snapshot_file_name{};

  const SkeletonTopology* topology{nullptr};
  WholeMeshLodManifest lod_full{};
  WholeMeshLodManifest lod_minimal{};

  std::span<const BakeClipDescriptor> clips{};
  std::span<const BakeSocketDescriptor> sockets{};
  BindPaletteProviderFn bind_palette{nullptr};
  CreatureSpecProviderFn creature_spec{nullptr};
  BakeClipFrameFn bake_clip_frame{nullptr};
  BakeClipMarkersFn clip_markers{nullptr};
};

struct CompiledWholeMeshLod {
  std::unique_ptr<Render::GL::Mesh> mesh{};
  std::vector<PrimitiveInstance> primitives{};

  [[nodiscard]] auto part_graph() const noexcept -> PartGraph {
    return {std::span<const PrimitiveInstance>(primitives)};
  }
};

[[nodiscard]] auto
compile_whole_mesh_lod(const WholeMeshLodManifest& manifest) -> CompiledWholeMeshLod;

} // namespace Render::Creature
