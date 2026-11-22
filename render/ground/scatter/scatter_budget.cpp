#include "render/ground/scatter/scatter_budget.h"

#include <algorithm>
#include <cmath>

namespace Render::Ground::Scatter {

auto scatter_instances_per_cell(Render::GL::ScatterSpeciesId species) -> float {
  using Render::GL::ScatterSpeciesId;
  switch (species) {
  case ScatterSpeciesId::Grass:
    return 0.0F;
  case ScatterSpeciesId::Stone:
    return 1.0F;
  case ScatterSpeciesId::Plant:
    return 4.0F;
  case ScatterSpeciesId::Pine:
  case ScatterSpeciesId::Olive:
  case ScatterSpeciesId::Cypress:
  case ScatterSpeciesId::Palm:
    return 4.0F;
  case ScatterSpeciesId::FireCamp:
  case ScatterSpeciesId::Tent:
  case ScatterSpeciesId::SupplyCart:
  case ScatterSpeciesId::WeaponRack:
  case ScatterSpeciesId::Ruins:
  case ScatterSpeciesId::DeadTree:
  case ScatterSpeciesId::Boulder:
  case ScatterSpeciesId::IronOre:
  case ScatterSpeciesId::MagicShrine:
  case ScatterSpeciesId::AbandonedHome:
  case ScatterSpeciesId::Statue:
  case ScatterSpeciesId::CursedGoldVein:
    return 1.0F;
  }
  return 1.0F;
}

auto scatter_instance_budget(Render::GL::ScatterSpeciesId species,
                             std::size_t grid_cells,
                             float grass_density) -> std::size_t {
  if (species == Render::GL::ScatterSpeciesId::Grass) {
    const float density =
        std::max(std::clamp(grass_density, 0.0F, 1.0F), k_min_grass_density_scale);
    const double budget = static_cast<double>(k_grass_blade_cap) *
                          static_cast<double>(k_grass_budget_headroom) *
                          static_cast<double>(density);
    return std::max(k_min_layer_budget, static_cast<std::size_t>(std::llround(budget)));
  }
  const auto per_cell = static_cast<std::size_t>(
      std::ceil(static_cast<float>(grid_cells) * scatter_instances_per_cell(species)));
  return std::max(k_min_layer_budget, per_cell);
}

} // namespace Render::Ground::Scatter
