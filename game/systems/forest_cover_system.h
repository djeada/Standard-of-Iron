#pragma once

#include <vector>

#include "../core/system.h"
#include "combat_system/combat_types.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems {

using Combat::Constants::k_forest_reveal_after_strike;
using Combat::Constants::k_forest_spot_distance;

class ForestCoverSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

private:
  static void spot(Engine::Core::World& world,
                   const std::vector<Engine::Core::Entity*>& covered);
  static void lose_track_of_the_hidden(Engine::Core::World& world);
};

} // namespace Game::Systems
