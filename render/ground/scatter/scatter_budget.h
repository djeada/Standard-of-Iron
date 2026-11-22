#pragma once

#include <cstddef>

#include "render/frame/terrain_scene_types.h"

namespace Render::Ground::Scatter {

// The most instances a scatter layer may hold on a map, given its grid size and
// the graphics profile's grass density. TerrainScatterManager owns the split:
// it hands each layer its budget before configuring, so total scatter memory is
// bounded by the sum (instances x instance size), and the benchmark JSON
// reports both the budget and whether any layer reached it.
//
// The budgets are ceilings, not detail settings. Each sits well above what the
// layer's generator can produce today, so no shipped map reaches one
// (scatter_generator_test checks every map in assets/maps):
//   - grass: 4x the background-blade cap the generator already shapes its
//     density to (1.1M blades at full density), scaled by the profile's grass
//     density like that cap. Patches peak at 2.2x the background density and
//     clusters and flowers add a few percent, so 4x leaves headroom. Grass is
//     shuffled before the cut, so a cut would thin it evenly;
//   - stones 1 and plants 4 per grid cell (they sample one cell in 4 or 16 and
//     add a few satellites per leader);
//   - each tree species 4 per cell; every world-prop layer 1 per cell (one
//     instance per prop);
//   - never less than k_min_layer_budget, so small maps are not squeezed.
inline constexpr std::size_t k_grass_blade_cap = 1'100'000U;
inline constexpr float k_grass_budget_headroom = 4.0F;
inline constexpr float k_min_grass_density_scale = 0.1F;
inline constexpr std::size_t k_min_layer_budget = 4096U;

[[nodiscard]] auto
scatter_instances_per_cell(Render::GL::ScatterSpeciesId species) -> float;

[[nodiscard]] auto scatter_instance_budget(Render::GL::ScatterSpeciesId species,
                                           std::size_t grid_cells,
                                           float grass_density) -> std::size_t;

} // namespace Render::Ground::Scatter
