#pragma once

#include "../../engine/core/system.h"

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
    void updateAI(Engine::Core::Entity* entity, float deltaTime);
};

} // namespace Game::Systems