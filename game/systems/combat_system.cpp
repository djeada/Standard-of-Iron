#include "combat_system.h"
#include "../../engine/core/world.h"
#include "../../engine/core/component.h"

namespace Game::Systems {

void CombatSystem::update(Engine::Core::World* world, float deltaTime) {
    processAttacks(world, deltaTime);
}

void CombatSystem::processAttacks(Engine::Core::World* world, float deltaTime) {
    auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();
    
    for (auto attacker : units) {
        auto attackerUnit = attacker->getComponent<Engine::Core::UnitComponent>();
        auto attackerTransform = attacker->getComponent<Engine::Core::TransformComponent>();
        
        if (!attackerUnit || !attackerTransform) {
            continue;
        }
        
        // Simple AI: attack nearby enemies
        for (auto target : units) {
            if (target == attacker) {
                continue;
            }
            
            auto targetUnit = target->getComponent<Engine::Core::UnitComponent>();
            if (!targetUnit || targetUnit->health <= 0) {
                continue;
            }
            
            if (isInRange(attacker, target, 2.0f)) {
                dealDamage(target, 10); // Simple damage system
                break; // Only attack one target per update
            }
        }
    }
}

bool CombatSystem::isInRange(Engine::Core::Entity* attacker, Engine::Core::Entity* target, float range) {
    auto attackerTransform = attacker->getComponent<Engine::Core::TransformComponent>();
    auto targetTransform = target->getComponent<Engine::Core::TransformComponent>();
    
    if (!attackerTransform || !targetTransform) {
        return false;
    }
    
    float dx = targetTransform->position.x - attackerTransform->position.x;
    float dz = targetTransform->position.z - attackerTransform->position.z;
    float distanceSquared = dx * dx + dz * dz;
    
    return distanceSquared <= range * range;
}

void CombatSystem::dealDamage(Engine::Core::Entity* target, int damage) {
    auto unit = target->getComponent<Engine::Core::UnitComponent>();
    if (unit) {
        unit->health = std::max(0, unit->health - damage);
    }
}

void AISystem::update(Engine::Core::World* world, float deltaTime) {
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    
    for (auto entity : entities) {
        updateAI(entity, deltaTime);
    }
}

void AISystem::updateAI(Engine::Core::Entity* entity, float deltaTime) {
    // Simple AI logic placeholder
    // In a real implementation, this would include:
    // - State machines
    // - Behavior trees
    // - Goal-oriented action planning
    // - Pathfinding integration
}

} // namespace Game::Systems