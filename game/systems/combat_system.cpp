#include "combat_system.h"
#include "../core/component.h"
#include "../visuals/team_colors.h"
#include "arrow_system.h"
#include "../core/world.h"
#include "../core/component.h"

namespace Game::Systems {

void CombatSystem::update(Engine::Core::World* world, float deltaTime) {
    processAttacks(world, deltaTime);
}

void CombatSystem::processAttacks(Engine::Core::World* world, float deltaTime) {
    auto units = world->getEntitiesWith<Engine::Core::UnitComponent>();

    // Find ArrowSystem if present
    ArrowSystem* arrowSys = nullptr;
    for (auto& sys : world->systems()) {
        arrowSys = dynamic_cast<ArrowSystem*>(sys.get());
        if (arrowSys) break;
    }

    for (auto attacker : units) {
        auto attackerUnit = attacker->getComponent<Engine::Core::UnitComponent>();
        auto attackerTransform = attacker->getComponent<Engine::Core::TransformComponent>();
        auto attackerAtk = attacker->getComponent<Engine::Core::AttackComponent>();

        if (!attackerUnit || !attackerTransform) {
            continue;
        }
        // Skip dead attackers
        if (attackerUnit->health <= 0) continue;
        // If there's no attack component, use default melee-like behavior
        float range = 2.0f;
        int damage = 10;
        float cooldown = 1.0f;
        float* tAccum = nullptr;
        float tmpAccum = 0.0f;
        if (attackerAtk) {
            range = std::max(0.1f, attackerAtk->range);
            damage = std::max(0, attackerAtk->damage);
            cooldown = std::max(0.05f, attackerAtk->cooldown);
            attackerAtk->timeSinceLast += deltaTime;
            tAccum = &attackerAtk->timeSinceLast;
        } else {
            tmpAccum += deltaTime;
            tAccum = &tmpAccum;
        }
        if (*tAccum < cooldown) {
            continue; // still cooling down
        }

        // Simple target selection: first valid target in range
        for (auto target : units) {
            if (target == attacker) {
                continue;
            }

            auto targetUnit = target->getComponent<Engine::Core::UnitComponent>();
            if (!targetUnit || targetUnit->health <= 0) {
                continue;
            }
            // Friendly-fire check: only attack units with a different owner
            if (targetUnit->ownerId == attackerUnit->ownerId) {
                continue;
            }

            if (isInRange(attacker, target, range)) {
                // Arrow visual: spawn arrow if ArrowSystem present
                if (arrowSys) {
                    auto attT = attacker->getComponent<Engine::Core::TransformComponent>();
                    auto tgtT = target->getComponent<Engine::Core::TransformComponent>();
                    auto attU = attacker->getComponent<Engine::Core::UnitComponent>();
                    QVector3D aPos(attT->position.x, attT->position.y, attT->position.z);
                    QVector3D tPos(tgtT->position.x, tgtT->position.y, tgtT->position.z);
                    QVector3D dir = (tPos - aPos).normalized();
                    // Raise bow height and offset a bit forward to avoid intersecting the capsule body
                    QVector3D start = aPos + QVector3D(0.0f, 0.6f, 0.0f) + dir * 0.35f;
                    QVector3D end   = tPos + QVector3D(0.0f, 0.5f, 0.0f);
                    QVector3D color = attU ? Game::Visuals::teamColorForOwner(attU->ownerId) : QVector3D(0.8f, 0.9f, 1.0f);
                    arrowSys->spawnArrow(start, end, color, 14.0f);
                }
                dealDamage(target, damage);
                *tAccum = 0.0f; // reset cooldown
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
        if (unit->health <= 0) {
            // Hide the renderable so dead units disappear
            if (auto* r = target->getComponent<Engine::Core::RenderableComponent>()) {
                r->visible = false;
            }
        }
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