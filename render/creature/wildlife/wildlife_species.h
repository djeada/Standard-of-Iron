#pragma once

#include "render/creature/pipeline/creature_visual_definition.h"
#include "render/creature/pipeline/species_contract.h"

namespace Render::Creature::Pipeline {

[[nodiscard]] auto
sheep_creature_visual_definition() noexcept -> const CreatureVisualDefinition&;

[[nodiscard]] auto
wolf_creature_visual_definition() noexcept -> const CreatureVisualDefinition&;

} // namespace Render::Creature::Pipeline

namespace Render::Wildlife {

// The sheep's and the wolf's contracts with the creature pipeline.
[[nodiscard]] auto
sheep_species_contract() noexcept -> const Render::Creature::Pipeline::SpeciesContract&;

[[nodiscard]] auto
wolf_species_contract() noexcept -> const Render::Creature::Pipeline::SpeciesContract&;

} // namespace Render::Wildlife
