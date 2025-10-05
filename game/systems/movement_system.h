#pragma once

#include "../core/component.h"
#include "../core/system.h"
#include "../core/world.h"

namespace Engine {
namespace Core {
class Entity;
}
} // namespace Engine

namespace Game::Systems {

class MovementSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;

private:
  void moveUnit(Engine::Core::Entity *entity, float deltaTime);
  bool hasReachedTarget(const Engine::Core::TransformComponent *transform,
                        const Engine::Core::MovementComponent *movement);
};

} // namespace Game::Systems