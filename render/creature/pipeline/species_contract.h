#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/unit_visual_spec.h"

namespace Render::Creature::Pipeline {

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

  std::uint8_t role_count{0U};
  std::int32_t material_id{0};
  BindPaletteFn bind_palette{nullptr};

  std::span<const std::size_t> foot_bones{};

  DescribeAssetsFn describe_assets{nullptr};
  CreatureAssetId default_asset{k_invalid_creature_asset};

  InitializeLodGeometryFn initialize_lod_geometry{nullptr};

  const WholeMeshBones* whole_mesh_bones{nullptr};
};

void register_species(const SpeciesContract& contract);

[[nodiscard]] auto
species_contract(CreatureKind kind) noexcept -> const SpeciesContract*;

[[nodiscard]] auto
registered_species() noexcept -> std::span<const SpeciesContract* const>;

void register_builtin_species();

} // namespace Render::Creature::Pipeline
