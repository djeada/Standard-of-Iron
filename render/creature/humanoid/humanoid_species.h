#pragma once

#include "render/creature/pipeline/species_contract.h"

namespace Render::Humanoid {

// The humanoid's contract with the creature pipeline: its assets (the base
// body and the sword, spear, skeleton and caster variants), bind palette and
// feet.
[[nodiscard]] auto humanoid_species_contract() noexcept
    -> const Render::Creature::Pipeline::SpeciesContract&;

} // namespace Render::Humanoid
