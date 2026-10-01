#pragma once

#include "../core/system.h"

namespace Game::Systems {

class SiegeTowerSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

class WallWalkSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

} // namespace Game::Systems
