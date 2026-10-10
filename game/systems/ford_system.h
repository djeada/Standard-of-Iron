#pragma once

#include "game/core/system.h"

namespace Engine::Core {
class World;
} // namespace Engine::Core

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

// Tracks which units stand in or beside a river ford and keeps their
// WadingComponent current: water depth, the ford's speed and exposure, and
// the chill icy water leaves behind. Runs in the Movement phase so the
// motor and combat read this tick's footing.
class FordSystem : public Engine::Core::System {
public:
  struct Services {
    Game::Map::TerrainService& terrain;
  };

  explicit FordSystem(Services services);

  void update(Engine::Core::World* world, float delta_time) override;

  // Distance from a troop's centre that still counts as "beside" a ford, so
  // the flank soldiers of a troop whose centre is on the bank still splash.
  static constexpr float k_beside_reach = 4.0F;

private:
  Services m_services;
};

} // namespace Game::Systems
