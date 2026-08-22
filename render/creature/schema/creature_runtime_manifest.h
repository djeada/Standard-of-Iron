#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "render/creature/part_graph.h"
#include "render/creature/quadruped/mesh_graph.h"
#include "render/creature/spec.h"

namespace Render::Creature {

using BindPaletteProviderFn = std::span<const QMatrix4x4> (*)() noexcept;
using CreatureSpecProviderFn = const CreatureSpec& (*)() noexcept;

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

struct CreatureRuntimeManifest {
  std::string_view species_name{};
  std::uint32_t species_id{0U};
  std::string_view bpat_file_name{};
  std::string_view minimal_snapshot_file_name{};

  const SkeletonTopology* topology{nullptr};
  WholeMeshLodManifest lod_full{};
  WholeMeshLodManifest lod_minimal{};

  BindPaletteProviderFn bind_palette{nullptr};
  CreatureSpecProviderFn creature_spec{nullptr};
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
