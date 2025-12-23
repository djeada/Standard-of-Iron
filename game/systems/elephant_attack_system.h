#pragma once

#include "../core/system.h"

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Game::Systems {

class ElephantAttackSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  void process_elephant_attacks(Engine::Core::World *world, float delta_time);
  void process_charge_mechanics(Engine::Core::Entity *elephant,
                                 Engine::Core::World *world, float delta_time);
  void process_trample_damage(Engine::Core::Entity *elephant,
                              Engine::Core::World *world, float delta_time);
  void process_panic_behavior(Engine::Core::Entity *elephant,
                              Engine::Core::World *world, float delta_time);
};

} // namespace Game::Systems
