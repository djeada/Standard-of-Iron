#pragma once

#include "../core/system.h"

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Game::Systems {

class CatapultAttackSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  void process_catapult_attacks(Engine::Core::World *world, float delta_time);
  void start_loading(Engine::Core::Entity *catapult,
                     Engine::Core::Entity *target);
  void update_loading(Engine::Core::Entity *catapult, float delta_time);
  void fire_projectile(Engine::Core::World *world,
                       Engine::Core::Entity *catapult);
  void update_firing(Engine::Core::Entity *catapult, float delta_time);
};

} // namespace Game::Systems
