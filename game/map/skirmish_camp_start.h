#pragma once

#include <vector>

#include "core/entity_id.h"

namespace Engine::Core {
class World;
}

namespace Game::Map {

// What every seat of a skirmish starts with around its barracks, on top of what
// the map authors (docs/design/PLAY_BIG_SOON.md, "Camp start").
struct CampStart {
  int homes = 2;
  int farms = 1;
  int builder_crews = 3;
  int barracks_reserve = 200;
};

struct CampStartReport {
  int owner_id = 0;
  Engine::Core::EntityID barracks = 0;
  std::vector<Engine::Core::EntityID> homes;
  std::vector<Engine::Core::EntityID> farms;
  std::vector<Engine::Core::EntityID> builders;
  std::vector<Engine::Core::EntityID> squads;
};

// Raises the camp for every player and computer seat that owns a barracks: homes
// and a farm behind it, builder crews up to the camp's count, a line squad and a
// missile squad of the seat's nation at its gate, and a deeper barracks reserve.
// Anything that finds no clear ground is skipped, never stacked. Call once, after
// the map's units and the navigation grid exist and before the match runs.
auto apply_skirmish_camp_start(Engine::Core::World& world, const CampStart& camp = {})
    -> std::vector<CampStartReport>;

} // namespace Game::Map
