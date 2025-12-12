#pragma once

#include "../core/system.h"

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Game::Systems {

class BallistaAttackSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  void process_ballista_attacks(Engine::Core::World *world, float delta_time);
  void start_loading(Engine::Core::Entity *ballista,
                     Engine::Core::Entity *target);
  void update_loading(Engine::Core::Entity *ballista, float delta_time);
  void fire_projectile(Engine::Core::World *world,
                       Engine::Core::Entity *ballista);
  void update_firing(Engine::Core::Entity *ballista, float delta_time);
};

} // namespace Game::Systems
