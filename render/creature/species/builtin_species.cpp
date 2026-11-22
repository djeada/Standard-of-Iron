// The one place that names every built-in species. Adding a species means a
// folder under render/creature/ with its SpeciesContract, plus one line here.

#include "render/creature/elephant/elephant_species.h"
#include "render/creature/horse/horse_species.h"
#include "render/creature/humanoid/humanoid_species.h"
#include "render/creature/pipeline/species_contract.h"
#include "render/creature/wildlife/wildlife_species.h"

namespace Render::Creature::Pipeline {

void register_builtin_species() {
  register_species(Render::Humanoid::humanoid_species_contract());
  register_species(Render::Horse::horse_species_contract());
  register_species(Render::Elephant::elephant_species_contract());
  register_species(Render::Wildlife::sheep_species_contract());
  register_species(Render::Wildlife::wolf_species_contract());
}

} // namespace Render::Creature::Pipeline
