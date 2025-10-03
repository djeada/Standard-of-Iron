#pragma once

#include "../core/system.h"

namespace Engine { namespace Core { class Entity; } }

namespace Game::Systems {

class CombatSystem : public Engine::Core::System {
public:
    void update(Engine::Core::World* world, float deltaTime) override;

private:
    void processAttacks(Engine::Core::World* world, float deltaTime);
    bool isInRange(Engine::Core::Entity* attacker, Engine::Core::Entity* target, float range);
    void dealDamage(Engine::Core::Entity* target, int damage);
};

class AISystem : public Engine::Core::System {
public:
    void update(Engine::Core::World* world, float deltaTime) override;

private:
    void updateAI(Engine::Core::World* world, float deltaTime);
    void updateProductionAI(Engine::Core::World* world, float deltaTime);
    void updateCombatAI(Engine::Core::World* world, float deltaTime);
    
    float m_productionTimer = 0.0f;
    float m_combatTimer = 0.0f;
};

} // namespace Game::Systems