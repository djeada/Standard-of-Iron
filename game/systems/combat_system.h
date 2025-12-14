#pragma once

#include "../core/entity.h"
#include "../core/system.h"
#include <unordered_map>

namespace Engine::Core {
class AttackComponent;
}

namespace Game::Systems {

class CombatSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  static void process_attacks(Engine::Core::World *world, float delta_time);
  static void updateCombatMode(Engine::Core::Entity *attacker,
                               Engine::Core::World *world,
                               Engine::Core::AttackComponent *attack_comp);
  static auto is_in_range(Engine::Core::Entity *attacker,
                          Engine::Core::Entity *target, float range) -> bool;
  static void dealDamage(Engine::Core::World *world,
                         Engine::Core::Entity *target, int damage,
                         Engine::Core::EntityID attackerId = 0);
  void process_auto_engagement(Engine::Core::World *world, float delta_time);
  static auto is_unit_idle(Engine::Core::Entity *unit) -> bool;
  static auto findNearestEnemy(Engine::Core::Entity *unit,
                               Engine::Core::World *world,
                               float max_range) -> Engine::Core::Entity *;

  std::unordered_map<Engine::Core::EntityID, float> m_engagementCooldowns;
  static constexpr float ENGAGEMENT_COOLDOWN = 0.5F;
};

} // namespace Game::Systems
