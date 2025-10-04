#include "patrol_system.h"
#include "../core/world.h"
#include "../core/component.h"
#include <QVector3D>
#include <cmath>

namespace Game::Systems {

void PatrolSystem::update(Engine::Core::World* world, float deltaTime) {
    if (!world) return;
    
    auto entities = world->getEntitiesWith<Engine::Core::PatrolComponent>();
    
    for (auto* entity : entities) {
        auto* patrol = entity->getComponent<Engine::Core::PatrolComponent>();
        auto* movement = entity->getComponent<Engine::Core::MovementComponent>();
        auto* transform = entity->getComponent<Engine::Core::TransformComponent>();
        auto* unit = entity->getComponent<Engine::Core::UnitComponent>();
        
        if (!patrol || !movement || !transform || !unit) continue;
        if (!patrol->patrolling || patrol->waypoints.size() < 2) continue;
        
        // Stop patrolling if unit is dead
        if (unit->health <= 0) {
            patrol->patrolling = false;
            continue;
        }
        
        // Check if unit is under attack or attacking - pause patrol
        auto* attackTarget = entity->getComponent<Engine::Core::AttackTargetComponent>();
        if (attackTarget && attackTarget->targetId != 0) {
            // Unit is attacking something, pause patrol
            continue;
        }
        
        // Check if enemies are nearby - engage them
        bool enemyNearby = false;
        auto allEntities = world->getEntitiesWith<Engine::Core::UnitComponent>();
        for (auto* other : allEntities) {
            auto* otherUnit = other->getComponent<Engine::Core::UnitComponent>();
            auto* otherTransform = other->getComponent<Engine::Core::TransformComponent>();
            
            if (!otherUnit || !otherTransform || otherUnit->health <= 0) continue;
            if (otherUnit->ownerId == unit->ownerId) continue; // Skip allies
            
            // Calculate distance
            float dx = otherTransform->position.x - transform->position.x;
            float dz = otherTransform->position.z - transform->position.z;
            float distSq = dx * dx + dz * dz;
            
            // If enemy within 5 units, engage
            if (distSq < 25.0f) {
                enemyNearby = true;
                // Set attack target
                if (!attackTarget) {
                    entity->addComponent<Engine::Core::AttackTargetComponent>();
                    attackTarget = entity->getComponent<Engine::Core::AttackTargetComponent>();
                }
                if (attackTarget) {
                    attackTarget->targetId = other->getId();
                    attackTarget->shouldChase = false; // Don't chase far, return to patrol
                }
                break;
            }
        }
        
        if (enemyNearby) {
            // Pause patrol to engage enemy
            continue;
        }
        
        // Continue patrol - move to current waypoint
        auto waypoint = patrol->waypoints[patrol->currentWaypoint];
        float targetX = waypoint.first;
        float targetZ = waypoint.second;
        
        // Check if reached current waypoint
        float dx = targetX - transform->position.x;
        float dz = targetZ - transform->position.z;
        float distSq = dx * dx + dz * dz;
        
        if (distSq < 1.0f) { // Reached waypoint
            // Move to next waypoint
            patrol->currentWaypoint = (patrol->currentWaypoint + 1) % patrol->waypoints.size();
            waypoint = patrol->waypoints[patrol->currentWaypoint];
            targetX = waypoint.first;
            targetZ = waypoint.second;
        }
        
        // Set movement target to current waypoint
        movement->hasTarget = true;
        movement->targetX = targetX;
        movement->targetY = targetZ;
    }
}

} // namespace Game::Systems
