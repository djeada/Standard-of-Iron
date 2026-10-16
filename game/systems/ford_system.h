#pragma once

#include "game/core/system.h"

namespace Engine::Core {
class World;
} // namespace Engine::Core

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

class FordSystem : public Engine::Core::System {
public:
  struct Services {
    Game::Map::TerrainService& terrain;
  };

  explicit FordSystem(Services services);

  void update(Engine::Core::World* world, float delta_time) override;

  static constexpr float k_beside_reach = 4.0F;

private:
  Services m_services;
};

} // namespace Game::Systems
