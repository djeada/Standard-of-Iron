#include "ai_system.h"
#include "../core/world.h"
#include "../core/component.h"
#include "command_service.h"
#include "formation_planner.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Game::Systems {

// ============================================================================
// AISystem Implementation
// ============================================================================

AISystem::AISystem() {
    // Initialize enemy AI context
    m_enemyAI.playerId = 2;
    m_enemyAI.state = AIState::Idle;
    
    // Register default behaviors
    registerBehavior(std::make_unique<ProductionBehavior>());
    registerBehavior(std::make_unique<GatherBehavior>());
    registerBehavior(std::make_unique<AttackBehavior>());
    registerBehavior(std::make_unique<DefendBehavior>());
}

AISystem::~AISystem() = default;

void AISystem::registerBehavior(std::unique_ptr<AIBehavior> behavior) {
    m_behaviors.push_back(std::move(behavior));
}

void AISystem::update(Engine::Core::World* world, float deltaTime) {
    if (!world) return;
    
    // Global update rate limiting (update AI decisions at reasonable intervals)
    m_globalUpdateTimer += deltaTime;
    if (m_globalUpdateTimer < 0.5f) return; // Update twice per second
    deltaTime = m_globalUpdateTimer;
    m_globalUpdateTimer = 0.0f;
    
    // Update AI context with current world state
    updateContext(world, m_enemyAI);
    
    // Run state machine
    updateStateMachine(world, m_enemyAI, deltaTime);
    
    // Execute behaviors
    for (auto& behavior : m_behaviors) {
        if (behavior && behavior->shouldExecute(world, m_enemyAI.playerId)) {
            behavior->execute(world, m_enemyAI.playerId, deltaTime);
        }
    }
}

void AISystem::updateContext(Engine::Core::World* world, AIContext& ctx) {
    // Clear previous frame data
    ctx.militaryUnits.clear();
    ctx.buildings.clear();
    ctx.totalUnits = 0;
    ctx.idleUnits = 0;
    ctx.combatUnits = 0;
    float totalHealthRatio = 0.0f;
    
    // Scan all entities and categorize
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != ctx.playerId || u->health <= 0) continue;
        
        if (e->hasComponent<Engine::Core::BuildingComponent>()) {
            ctx.buildings.push_back(e->getId());
            if (u->unitType == "barracks" && ctx.primaryBarracks == 0) {
                ctx.primaryBarracks = e->getId();
                // Set rally point near barracks
                if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                    ctx.rallyX = t->position.x - 5.0f;
                    ctx.rallyZ = t->position.z;
                }
            }
        } else {
            ctx.militaryUnits.push_back(e->getId());
            ctx.totalUnits++;
            
            // Check if unit is idle or in combat
            auto* m = e->getComponent<Engine::Core::MovementComponent>();
            if (!m || !m->hasTarget) {
                ctx.idleUnits++;
            } else {
                ctx.combatUnits++;
            }
            
            // Track health
            if (u->maxHealth > 0) {
                totalHealthRatio += float(u->health) / float(u->maxHealth);
            }
        }
    }
    
    // Calculate average health
    if (ctx.totalUnits > 0) {
        ctx.averageHealth = totalHealthRatio / float(ctx.totalUnits);
    } else {
        ctx.averageHealth = 1.0f;
    }
}

void AISystem::updateStateMachine(Engine::Core::World* world, AIContext& ctx, float deltaTime) {
    ctx.stateTimer += deltaTime;
    ctx.decisionTimer += deltaTime;
    
    // Make state decisions every 5 seconds
    if (ctx.decisionTimer < 5.0f) return;
    ctx.decisionTimer = 0.0f;
    
    AIState previousState = ctx.state;
    
    // State transition logic
    switch (ctx.state) {
        case AIState::Idle:
            // Gather units if we have idle units
            if (ctx.idleUnits >= 3) {
                ctx.state = AIState::Gathering;
            }
            // Defend if under attack (low health)
            else if (ctx.averageHealth < 0.5f && ctx.totalUnits > 0) {
                ctx.state = AIState::Defending;
            }
            break;
            
        case AIState::Gathering:
            // Once we have enough units, attack
            if (ctx.totalUnits >= 5 && ctx.idleUnits < 2) {
                ctx.state = AIState::Attacking;
            }
            // Go idle if not enough units
            else if (ctx.totalUnits < 2) {
                ctx.state = AIState::Idle;
            }
            break;
            
        case AIState::Attacking:
            // Retreat if heavily damaged
            if (ctx.averageHealth < 0.3f) {
                ctx.state = AIState::Retreating;
            }
            // Return to gathering if we lost too many units
            else if (ctx.totalUnits < 3) {
                ctx.state = AIState::Gathering;
            }
            break;
            
        case AIState::Defending:
            // Return to normal operations if healthy again
            if (ctx.averageHealth > 0.7f) {
                ctx.state = AIState::Idle;
            }
            // Attack if we have enough healthy units
            else if (ctx.totalUnits >= 5 && ctx.averageHealth > 0.5f) {
                ctx.state = AIState::Attacking;
            }
            break;
            
        case AIState::Retreating:
            // Defend base when we arrive
            if (ctx.stateTimer > 8.0f) {
                ctx.state = AIState::Defending;
            }
            break;
            
        case AIState::Expanding:
            // Not implemented yet, return to idle
            ctx.state = AIState::Idle;
            break;
    }
    
    // Reset state timer on transition
    if (ctx.state != previousState) {
        ctx.stateTimer = 0.0f;
    }
}

void AISystem::executeCurrentState(Engine::Core::World* world, AIContext& ctx, float deltaTime) {
    // State-specific high-level commands handled by behaviors
    // This method can be extended for complex state-specific logic
}

// ============================================================================
// ProductionBehavior Implementation
// ============================================================================

void ProductionBehavior::execute(Engine::Core::World* world, int playerId, float deltaTime) {
    m_productionTimer += deltaTime;
    if (m_productionTimer < 2.0f) return;
    m_productionTimer = 0.0f;
    
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (u->unitType != "barracks") continue;
        
        auto* prod = e->getComponent<Engine::Core::ProductionComponent>();
        if (!prod) continue;
        
        // Start production if not already producing and under cap
        if (!prod->inProgress && prod->producedCount < prod->maxUnits) {
            prod->productType = "archer";
            prod->timeRemaining = prod->buildTime;
            prod->inProgress = true;
        }
    }
}

bool ProductionBehavior::shouldExecute(Engine::Core::World* world, int playerId) const {
    // Always try to produce units
    return true;
}

// ============================================================================
// GatherBehavior Implementation
// ============================================================================

void GatherBehavior::execute(Engine::Core::World* world, int playerId, float deltaTime) {
    m_gatherTimer += deltaTime;
    if (m_gatherTimer < 4.0f) return;
    m_gatherTimer = 0.0f;
    
    // Find rally point (near barracks)
    QVector3D rallyPoint(0.0f, 0.0f, 0.0f);
    bool foundBarracks = false;
    
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (u->unitType == "barracks") {
            if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                rallyPoint = QVector3D(t->position.x - 5.0f, 0.0f, t->position.z);
                foundBarracks = true;
                break;
            }
        }
    }
    
    if (!foundBarracks) return;
    
    // Gather idle units to rally point
    std::vector<Engine::Core::EntityID> idleUnits;
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (e->hasComponent<Engine::Core::BuildingComponent>()) continue;
        
        auto* m = e->getComponent<Engine::Core::MovementComponent>();
        if (!m || m->hasTarget) continue;
        
        idleUnits.push_back(e->getId());
    }
    
    if (idleUnits.empty()) return;
    
    // Move units to rally point in formation
    auto targets = FormationPlanner::spreadFormation(int(idleUnits.size()), rallyPoint, 1.2f);
    CommandService::moveUnits(*world, idleUnits, targets);
}

bool GatherBehavior::shouldExecute(Engine::Core::World* world, int playerId) const {
    // Execute when we have idle units
    int idleCount = 0;
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (e->hasComponent<Engine::Core::BuildingComponent>()) continue;
        
        auto* m = e->getComponent<Engine::Core::MovementComponent>();
        if (!m || !m->hasTarget) idleCount++;
    }
    return idleCount >= 2;
}

// ============================================================================
// AttackBehavior Implementation
// ============================================================================

void AttackBehavior::execute(Engine::Core::World* world, int playerId, float deltaTime) {
    m_attackTimer += deltaTime;
    if (m_attackTimer < 3.0f) return;
    m_attackTimer = 0.0f;
    
    // Find enemy targets (prioritize buildings, then units)
    std::vector<Engine::Core::Entity*> enemyBuildings;
    std::vector<Engine::Core::Entity*> enemyUnits;
    
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId == playerId || u->health <= 0) continue;
        
        if (e->hasComponent<Engine::Core::BuildingComponent>()) {
            enemyBuildings.push_back(e);
        } else {
            enemyUnits.push_back(e);
        }
    }
    
    // Choose target (prefer buildings for strategic victory)
    Engine::Core::Entity* mainTarget = nullptr;
    if (!enemyBuildings.empty()) {
        mainTarget = enemyBuildings[0]; // Target first building (usually barracks)
    } else if (!enemyUnits.empty()) {
        mainTarget = enemyUnits[0];
    }
    
    if (!mainTarget) return;
    
    // Gather our units
    std::vector<Engine::Core::EntityID> attackers;
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (e->hasComponent<Engine::Core::BuildingComponent>()) continue;
        
        attackers.push_back(e->getId());
    }
    
    if (attackers.empty()) return;
    
    // Issue attack command on the target building
    CommandService::attackTarget(*world, attackers, mainTarget->getId(), true);
}

bool AttackBehavior::shouldExecute(Engine::Core::World* world, int playerId) const {
    // Attack when we have sufficient forces
    int unitCount = 0;
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (!e->hasComponent<Engine::Core::BuildingComponent>()) unitCount++;
    }
    return unitCount >= 4;
}

// ============================================================================
// DefendBehavior Implementation
// ============================================================================

void DefendBehavior::execute(Engine::Core::World* world, int playerId, float deltaTime) {
    m_defendTimer += deltaTime;
    if (m_defendTimer < 3.0f) return;
    m_defendTimer = 0.0f;
    
    // Find our barracks to defend
    QVector3D defendPos(0.0f, 0.0f, 0.0f);
    bool foundBarracks = false;
    
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (u->unitType == "barracks") {
            if (auto* t = e->getComponent<Engine::Core::TransformComponent>()) {
                defendPos = QVector3D(t->position.x, 0.0f, t->position.z);
                foundBarracks = true;
                break;
            }
        }
    }
    
    if (!foundBarracks) return;
    
    // Gather defenders
    std::vector<Engine::Core::EntityID> defenders;
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (e->hasComponent<Engine::Core::BuildingComponent>()) continue;
        
        defenders.push_back(e->getId());
    }
    
    if (defenders.empty()) return;
    
    // Position defenders around barracks in defensive formation
    auto targets = FormationPlanner::spreadFormation(int(defenders.size()), defendPos, 3.0f);
    CommandService::moveUnits(*world, defenders, targets);
}

bool DefendBehavior::shouldExecute(Engine::Core::World* world, int playerId) const {
    // Defend when units are damaged or under threat
    float totalHealth = 0.0f;
    int count = 0;
    
    auto entities = world->getEntitiesWith<Engine::Core::UnitComponent>();
    for (auto* e : entities) {
        auto* u = e->getComponent<Engine::Core::UnitComponent>();
        if (!u || u->ownerId != playerId || u->health <= 0) continue;
        if (e->hasComponent<Engine::Core::BuildingComponent>()) continue;
        
        if (u->maxHealth > 0) {
            totalHealth += float(u->health) / float(u->maxHealth);
            count++;
        }
    }
    
    if (count == 0) return false;
    float avgHealth = totalHealth / float(count);
    return avgHealth < 0.6f; // Defend when average health below 60%
}

} // namespace Game::Systems