#include "combat_system.h"
#include "../core/component.h"
#include "../visuals/team_colors.h"
#include "arrow_system.h"
#include "../core/world.h"
#include "../core/component.h"
#include <limits>
#include <algorithm>

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

        // Target selection: prioritize enemy units, but also attack enemy buildings
        Engine::Core::Entity* bestTarget = nullptr;
        
        // First pass: look for enemy units (non-buildings)
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
            
            // Skip buildings in first pass
            if (target->hasComponent<Engine::Core::BuildingComponent>()) {
                continue;
            }

            if (isInRange(attacker, target, range)) {
                bestTarget = target;
                break;
            }
        }
        
        // Second pass: if no units found, target enemy buildings
        if (!bestTarget) {
            for (auto target : units) {
                if (target == attacker) {
                    continue;
                }

                auto targetUnit = target->getComponent<Engine::Core::UnitComponent>();
                if (!targetUnit || targetUnit->health <= 0) {
                    continue;
                }
                // Friendly-fire check
                if (targetUnit->ownerId == attackerUnit->ownerId) {
                    continue;
                }
                
                // Only consider buildings in second pass
                if (!target->hasComponent<Engine::Core::BuildingComponent>()) {
                    continue;
                }

                if (isInRange(attacker, target, range)) {
                    bestTarget = target;
                    break;
                }
            }
        }
        
        // Attack the selected target
        if (bestTarget) {
            // Arrow visual: spawn arrow if ArrowSystem present
            if (arrowSys) {
                auto attT = attacker->getComponent<Engine::Core::TransformComponent>();
                auto tgtT = bestTarget->getComponent<Engine::Core::TransformComponent>();
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
            dealDamage(bestTarget, damage);
            *tAccum = 0.0f; // reset cooldown
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
    updateProductionAI(world, deltaTime);
    updateCombatAI(world, deltaTime);
}

void AISystem::updateProductionAI(Engine::Core::World* world, float deltaTime) {
    if (!world) return;
    
    // Check production every 2 seconds
    m_productionTimer += deltaTime;
    if (m_productionTimer < 2.0f) return;
    m_productionTimer = 0.0f;
    
    // Find all enemy barracks (ownerId == 2)
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != 2) continue; // AI is player 2
        if (u->unitType != "barracks" || u->health <= 0) continue;
        
        auto* prod = e->getComponent<Engine::Core::ProductionComponent>();
        if (!prod) continue;
        
        // If not producing and haven't reached cap, start producing
        if (!prod->inProgress && prod->producedCount < prod->maxUnits) {
            prod->productType = "archer";
            prod->timeRemaining = prod->buildTime;
            prod->inProgress = true;
        }
    }
}

void AISystem::updateCombatAI(Engine::Core::World* world, float deltaTime) {
    if (!world) return;
    
    // Command units every 3 seconds
    m_combatTimer += deltaTime;
    if (m_combatTimer < 3.0f) return;
    m_combatTimer = 0.0f;
    
    // Find all AI units (ownerId == 2)
    std::vector<Engine::Core::Entity*> aiUnits;
    std::vector<Engine::Core::Entity*> playerTargets;
    
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->health <= 0) continue;
        
        if (u->ownerId == 2 && u->unitType == "archer") {
            aiUnits.push_back(e);
        } else if (u->ownerId == 1) {
            // Target both player units and buildings
            playerTargets.push_back(e);
        }
    }
    
    if (aiUnits.empty() || playerTargets.empty()) return;
    
    // Simple AI strategy: attack nearest player target
    for (auto* aiUnit : aiUnits) {
        auto* t = aiUnit->getComponent<Engine::Core::TransformComponent>();
        auto* m = aiUnit->getComponent<Engine::Core::MovementComponent>();
        if (!t || !m) continue;
        
        // Skip if already moving
        if (m->hasTarget) continue;
        
        // Find nearest player target
        float minDist2 = std::numeric_limits<float>::max();
        Engine::Core::Entity* nearestTarget = nullptr;
        
        for (auto* target : playerTargets) {
            auto* targetT = target->getComponent<Engine::Core::TransformComponent>();
            if (!targetT) continue;
            
            float dx = targetT->position.x - t->position.x;
            float dz = targetT->position.z - t->position.z;
            float dist2 = dx * dx + dz * dz;
            
            if (dist2 < minDist2) {
                minDist2 = dist2;
                nearestTarget = target;
            }
        }
        
        // Move toward nearest target if found and not already in range
        if (nearestTarget && minDist2 > 4.0f) {
            auto* targetT = nearestTarget->getComponent<Engine::Core::TransformComponent>();
            m->hasTarget = true;
            m->targetX = targetT->position.x;
            m->targetY = targetT->position.z;
        }
    }
}

void AISystem::updateAI(Engine::Core::World* world, float deltaTime) {
    // Legacy method - kept for compatibility but now handled by update()
}

} // namespace Game::Systems