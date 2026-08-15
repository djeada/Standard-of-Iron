#include "default_content.h"

#include "../formation/formation_data_loader.h"
#include "../units/troop_catalog_loader.h"
#include "construction_cost_catalog.h"
#include "nation_registry.h"

namespace Game::Systems {

void initialize_default_content(NationRegistry& nations) {
  if (nations.initialized()) {
    return;
  }
  Game::Formation::FormationDataLoader::reset_to_builtin_defaults();
  Game::Units::TroopCatalogLoader::load_default_catalog();
  Game::Formation::FormationDataLoader::load_all();
  load_default_construction_catalog();
  nations.register_default_nations();
}

} // namespace Game::Systems
