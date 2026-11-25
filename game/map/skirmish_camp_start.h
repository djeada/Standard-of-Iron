#pragma once

#include <vector>

#include "core/entity_id.h"

namespace Engine::Core {
class World;
}

namespace Game::Map {

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

auto apply_skirmish_camp_start(Engine::Core::World& world, const CampStart& camp = {})
    -> std::vector<CampStartReport>;

} // namespace Game::Map
