#pragma once

#include "render/creature/pipeline/creature_visual_definition.h"
#include "render/creature/pipeline/species_contract.h"

namespace Render::Creature::Pipeline {

[[nodiscard]] auto
horse_creature_visual_definition() noexcept -> const CreatureVisualDefinition&;

} // namespace Render::Creature::Pipeline

namespace Render::Horse {

[[nodiscard]] auto
horse_species_contract() noexcept -> const Render::Creature::Pipeline::SpeciesContract&;

} // namespace Render::Horse
