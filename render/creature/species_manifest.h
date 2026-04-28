#pragma once

#include "part_graph.h"
#include "quadruped/mesh_graph.h"
#include "spec.h"

#include <QMatrix4x4>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Render::Creature {

struct BakeClipDescriptor {
  std::string_view name{};
  std::uint32_t frame_count{0U};
  float fps{0.0F};
  bool loops{false};
};

using BindPaletteProviderFn = std::span<const QMatrix4x4> (*)() noexcept;
using CreatureSpecProviderFn = const CreatureSpec &(*)() noexcept;
using BakeClipPaletteFn = void (*)(std::size_t clip_index,
                                   std::uint32_t frame_index,
                                   std::vector<QMatrix4x4> &out_palettes);

struct WholeMeshLodManifest {
  std::string_view primitive_name{};
  BoneIndex anchor_bone{kInvalidBone};
  MeshSkinning mesh_skinning{MeshSkinning::Rigid};
  std::uint8_t color_role{0U};
  int material_id{0};
  std::uint8_t lod_mask{kLodAll};
  std::span<const Quadruped::MeshNode> mesh_nodes{};
  std::span<const std::string_view> excluded_node_name_prefixes{};
  std::span<const PrimitiveInstance> overlay_primitives{};
};

struct SpeciesManifest {
  std::string_view species_name{};
  std::uint32_t species_id{0U};
  std::string_view bpat_file_name{};
  std::string_view minimal_snapshot_file_name{};

  const SkeletonTopology *topology{nullptr};
  WholeMeshLodManifest lod_full{};
  WholeMeshLodManifest lod_minimal{};

  std::span<const BakeClipDescriptor> clips{};
  BindPaletteProviderFn bind_palette{nullptr};
  CreatureSpecProviderFn creature_spec{nullptr};
  BakeClipPaletteFn bake_clip_palette{nullptr};
};

struct CompiledWholeMeshLod {
  std::unique_ptr<Render::GL::Mesh> mesh{};
  std::vector<PrimitiveInstance> primitives{};

  [[nodiscard]] auto part_graph() const noexcept -> PartGraph {
    return {std::span<const PrimitiveInstance>(primitives)};
  }
};

[[nodiscard]] auto compile_whole_mesh_lod(const WholeMeshLodManifest &manifest)
    -> CompiledWholeMeshLod;

} // namespace Render::Creature
