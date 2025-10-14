#pragma once

#include "../core/system.h"

namespace Engine {
namespace Core {
class Entity;
class AttackComponent;
} // namespace Core
} // namespace Engine

namespace Game::Systems {

class CombatSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;

private:
  void processAttacks(Engine::Core::World *world, float deltaTime);
  void updateCombatMode(Engine::Core::Entity *attacker,
                        Engine::Core::World *world,
                        Engine::Core::AttackComponent *attackComp);
  bool isInRange(Engine::Core::Entity *attacker, Engine::Core::Entity *target,
                 float range);
  void dealDamage(Engine::Core::World *world, Engine::Core::Entity *target,
                  int damage, Engine::Core::EntityID attackerId = 0);
};

} // namespace Game::Systems