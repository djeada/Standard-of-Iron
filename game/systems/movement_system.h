#pragma once

#include "../core/component.h"
#include "../core/system.h"
#include "../core/world.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

class MovementSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;

private:
  static void moveUnit(Engine::Core::Entity *entity, Engine::Core::World *world,
                       float deltaTime);
  static auto
  hasReachedTarget(const Engine::Core::TransformComponent *transform,
                   const Engine::Core::MovementComponent *movement) -> bool;
};

} // namespace Game::Systems