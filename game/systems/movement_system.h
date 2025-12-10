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
  void update(Engine::Core::World *world, float delta_time) override;

private:
  static void move_unit(Engine::Core::Entity *entity, Engine::Core::World *world,
                       float delta_time);
  static auto
  has_reached_target(const Engine::Core::TransformComponent *transform,
                   const Engine::Core::MovementComponent *movement) -> bool;
};

} // namespace Game::Systems