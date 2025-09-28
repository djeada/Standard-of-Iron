#include "movement_system.h"
#include <cmath>

namespace Game::Systems {

void MovementSystem::update(Engine::Core::World* world, float deltaTime) {
    auto entities = world->getEntitiesWith<Engine::Core::MovementComponent>();
    
    for (auto entity : entities) {
        moveUnit(entity, deltaTime);
    }
}

void MovementSystem::moveUnit(Engine::Core::Entity* entity, float deltaTime) {
    auto transform = entity->getComponent<Engine::Core::TransformComponent>();
    auto movement = entity->getComponent<Engine::Core::MovementComponent>();
    auto unit = entity->getComponent<Engine::Core::UnitComponent>();
    
    if (!transform || !movement || !unit) {
        return;
    }
    
    if (!movement->hasTarget) {
        return;
    }
    
    // Check if we've reached the target
    if (hasReachedTarget(transform, movement)) {
        movement->hasTarget = false;
        return;
    }
    
    // Calculate direction to target
    float dx = movement->targetX - transform->position.x;
    float dz = movement->targetY - transform->position.z; // Using z for 2D movement
    float distance = std::sqrt(dx * dx + dz * dz);
    
    if (distance > 0.001f) {
        // Normalize direction and apply speed
        float moveDistance = unit->speed * deltaTime;
        
        if (moveDistance >= distance) {
            // Snap to target if we would overshoot
            transform->position.x = movement->targetX;
            transform->position.z = movement->targetY;
            movement->hasTarget = false;
        } else {
            // Move towards target
            float normalizedDx = dx / distance;
            float normalizedDz = dz / distance;
            
            transform->position.x += normalizedDx * moveDistance;
            transform->position.z += normalizedDz * moveDistance;
        }
    }
}

bool MovementSystem::hasReachedTarget(const Engine::Core::TransformComponent* transform,
                                    const Engine::Core::MovementComponent* movement) {
    float dx = movement->targetX - transform->position.x;
    float dz = movement->targetY - transform->position.z;
    float distanceSquared = dx * dx + dz * dz;
    
    const float threshold = 0.1f; // 0.1 unit tolerance
    return distanceSquared < threshold * threshold;
}

} // namespace Game::Systems