#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/unit_visual_spec.h"

// What a species gives the generic creature pipeline. Each species fills one
// SpeciesContract in its own folder; render/creature/species/builtin_species.cpp
// registers them all. Generic creature code reaches a species only through
// its contract, by CreatureKind, so it includes nothing from the species
// folders (scripts/check-render-layers.py enforces that).

namespace Render::Creature::Pipeline {

// Bone indices for skinning one welded low-detail body mesh (horse and
// elephant): the trunk and head chain and the four leg chains, front-left,
// front-right, back-left, back-right.
struct WholeMeshLegBones {
  std::uint8_t shoulder{0U};
  std::uint8_t knee{0U};
  std::uint8_t foot{0U};
};

struct WholeMeshBones {
  std::uint8_t root{0U};
  std::uint8_t neck{0U};
  std::uint8_t head{0U};
  std::uint8_t trunk_tip{0U};
  std::array<WholeMeshLegBones, 4> legs{};
};

using DescribeAssetsFn = void (*)(std::span<CreatureAsset> assets);
using InitializeLodGeometryFn = void (*)();

struct SpeciesContract {
  CreatureKind kind{CreatureKind::Humanoid};

  // Describe: the body.
  std::uint8_t role_count{0U};
  std::int32_t material_id{0};
  BindPaletteFn bind_palette{nullptr};
  // The bones whose lowest point is the creature's contact with the ground.
  std::span<const std::size_t> foot_bones{};

  // Describe: the creature assets. Writes each asset the species provides into
  // assets[asset.id]; default_asset is the one a bare CreatureKind resolves to.
  DescribeAssetsFn describe_assets{nullptr};
  CreatureAssetId default_asset{k_invalid_creature_asset};

  // Bake: build the species' procedural LOD meshes before the first bake.
  InitializeLodGeometryFn initialize_lod_geometry{nullptr};
  // Bake: the bones for whole-mesh skinning, when the species has such a mesh.
  const WholeMeshBones* whole_mesh_bones{nullptr};
};

// Registers (or replaces) the contract for contract.kind. The contract must
// outlive every lookup.
void register_species(const SpeciesContract& contract);

// The contract for `kind`, or null (CreatureKind::Mounted has none). The first
// lookup registers the built-in species.
[[nodiscard]] auto
species_contract(CreatureKind kind) noexcept -> const SpeciesContract*;

// Every registered contract, one slot per CreatureKind; empty slots are null.
// Registers the built-in species first, like species_contract().
[[nodiscard]] auto
registered_species() noexcept -> std::span<const SpeciesContract* const>;

// Registers humanoid, horse, elephant, sheep and wolf. Defined in
// render/creature/species/builtin_species.cpp, the one file that names every
// species; species_contract() calls it once.
void register_builtin_species();

} // namespace Render::Creature::Pipeline
