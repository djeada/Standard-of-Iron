#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class GuardSystem {
public:
  static void update(Engine::Core::World *world, float delta_time);
};

} // namespace Game::Systems
