#pragma once

#include "../core/entity.h"
#include "../core/system.h"
#include <unordered_map>

namespace Engine {
namespace Core {
class AttackComponent;
}
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
  void processAutoEngagement(Engine::Core::World *world, float deltaTime);
  bool isUnitIdle(Engine::Core::Entity *unit);
  Engine::Core::Entity *findNearestEnemy(Engine::Core::Entity *unit,
                                         Engine::Core::World *world,
                                         float maxRange);

  std::unordered_map<Engine::Core::EntityID, float> m_engagementCooldowns;
  static constexpr float ENGAGEMENT_COOLDOWN = 0.5f;
};

} // namespace Game::Systems