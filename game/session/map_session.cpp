#include "map_session.h"

#include "../core/world.h"
#include "../map/map_definition.h"
#include "../systems/cursed_gold_vein_system.h"
#include "../systems/undead_awakening_system.h"
#include "../systems/victory_service.h"
#include "../wildlife/wildlife_system.h"

namespace Game::Session {

void configure_map_systems(Engine::Core::World& world,
                           const Game::Map::MapDefinition& map_definition,
                           Game::Systems::VictoryService* victory_service) {
  if (auto* undead_system = world.get_system<Game::Systems::UndeadAwakeningSystem>()) {
    undead_system->configure(map_definition);
    if (victory_service != nullptr) {
      victory_service->set_undead_zone_query(undead_system);
    }
  }
  if (auto* vein_system = world.get_system<Game::Systems::CursedGoldVeinSystem>()) {
    vein_system->configure(map_definition);
  }
  if (auto* wildlife_system = world.get_system<Game::Wildlife::WildlifeSystem>()) {
    wildlife_system->configure(map_definition);
  }
}

auto restore_map_session(const MapSessionRestore& restore) -> SnapshotRestoreReport {
  if (restore.world != nullptr && restore.map != nullptr) {
    configure_map_systems(*restore.world, *restore.map, restore.victory_service);
  }
  if (restore.configure_victory_rules) {
    restore.configure_victory_rules();
  }
  return SessionSnapshot::restore(
      SnapshotScope{.world = restore.world, .map = restore.map}, restore.snapshot);
}

} // namespace Game::Session
