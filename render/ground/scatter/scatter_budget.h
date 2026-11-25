#pragma once

#include <cstddef>

#include "render/frame/terrain_scene_types.h"

namespace Render::Ground::Scatter {

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
