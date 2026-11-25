#pragma once

#include "render/creature/pipeline/species_contract.h"

namespace Render::Humanoid {

[[nodiscard]] auto humanoid_species_contract() noexcept
    -> const Render::Creature::Pipeline::SpeciesContract&;

} // namespace Render::Humanoid
