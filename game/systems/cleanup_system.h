#pragma once

#include "../core/entity.h"
#include "../core/system.h"

namespace Game::Systems {

class CleanupSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  static void remove_dead_entities(Engine::Core::World *world);
};

} // namespace Game::Systems
