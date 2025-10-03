#pragma once

#include "../core/system.h"
#include <vector>
#include <memory>

namespace Engine { namespace Core { 
    class World; 
    class Entity;
    using EntityID = unsigned int;
} }

namespace Game::Systems {

// AI state for a faction/player
enum class AIState {
    Idle,
    Gathering,      // Collecting units at rally point
    Attacking,      // Offensive push
    Defending,      // Protecting base
    Retreating,     // Pulling back damaged units
    Expanding       // Building new structures
};

// AI behavior interface for modularity
class AIBehavior {
public:
    virtual ~AIBehavior() = default;
    virtual void execute(Engine::Core::World* world, int playerId, float deltaTime) = 0;
    virtual bool shouldExecute(Engine::Core::World* world, int playerId) const = 0;
};

// AI context for a player/faction
struct AIContext {
    int playerId = 2;
    AIState state = AIState::Idle;
    float stateTimer = 0.0f;
    float decisionTimer = 0.0f;
    
    // Strategic data
    std::vector<Engine::Core::EntityID> militaryUnits;
    std::vector<Engine::Core::EntityID> buildings;
    Engine::Core::EntityID primaryBarracks = 0;
    
    // Tactical data
    float rallyX = 0.0f;
    float rallyZ = 0.0f;
    int targetPriority = 0; // 0=units, 1=buildings
    
    // Statistics
    int totalUnits = 0;
    int idleUnits = 0;
    int combatUnits = 0;
    float averageHealth = 1.0f;
};

// Main AI system with modular architecture
class AISystem : public Engine::Core::System {
public:
    AISystem();
    ~AISystem() override;
    
    void update(Engine::Core::World* world, float deltaTime) override;
    
    // Register AI behaviors
    void registerBehavior(std::unique_ptr<AIBehavior> behavior);
    
private:
    void updateContext(Engine::Core::World* world, AIContext& ctx);
    void updateStateMachine(Engine::Core::World* world, AIContext& ctx, float deltaTime);
    void executeCurrentState(Engine::Core::World* world, AIContext& ctx, float deltaTime);
    
    AIContext m_enemyAI;
    std::vector<std::unique_ptr<AIBehavior>> m_behaviors;
    float m_globalUpdateTimer = 0.0f;
};

// Concrete AI behaviors
class ProductionBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
private:
    float m_productionTimer = 0.0f;
};

class GatherBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
private:
    float m_gatherTimer = 0.0f;
};

class AttackBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
private:
    float m_attackTimer = 0.0f;
};

class DefendBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
private:
    float m_defendTimer = 0.0f;
};

} // namespace Game::Systems
