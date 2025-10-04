#pragma once

#include "../core/system.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

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

// Behavior priority levels (higher = more important)
enum class BehaviorPriority {
    VeryLow = 0,
    Low = 1,
    Normal = 2,
    High = 3,
    Critical = 4
};

// AI behavior interface for modularity
class AIBehavior {
public:
    virtual ~AIBehavior() = default;
    virtual void execute(Engine::Core::World* world, int playerId, float deltaTime) = 0;
    virtual bool shouldExecute(Engine::Core::World* world, int playerId) const = 0;
    virtual BehaviorPriority getPriority() const = 0;
    virtual bool canRunConcurrently() const { return false; } // Can this behavior run alongside others?
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
    
    // Concurrency control
    std::mutex contextMutex;
    std::atomic<bool> isProcessing{false};
};

// Main AI system with modular architecture and threading
class AISystem : public Engine::Core::System {
public:
    AISystem();
    ~AISystem() override;
    
    void update(Engine::Core::World* world, float deltaTime) override;
    
    // Register AI behaviors
    void registerBehavior(std::unique_ptr<AIBehavior> behavior);
    
    // Enable/disable threaded AI processing
    void setThreadedMode(bool enabled) { m_useThreading = enabled; }
    
private:
    void updateContext(Engine::Core::World* world, AIContext& ctx);
    void updateStateMachine(Engine::Core::World* world, AIContext& ctx, float deltaTime);
    void executeBehaviors(Engine::Core::World* world, float deltaTime);
    void executeBehaviorsThreaded(Engine::Core::World* world, float deltaTime);
    
    AIContext m_enemyAI;
    std::vector<std::unique_ptr<AIBehavior>> m_behaviors;
    float m_globalUpdateTimer = 0.0f;
    bool m_useThreading = false;
    
    // Threading support
    std::unique_ptr<std::thread> m_aiThread;
    std::condition_variable m_aiCondition;
    std::mutex m_aiMutex;
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_hasWork{false};
};

// Concrete AI behaviors with priorities
class ProductionBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
    BehaviorPriority getPriority() const override { return BehaviorPriority::High; } // Keep producing
    bool canRunConcurrently() const override { return true; } // Production is independent
private:
    float m_productionTimer = 0.0f;
};

class GatherBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
    BehaviorPriority getPriority() const override { return BehaviorPriority::Low; } // Low priority, passive
    bool canRunConcurrently() const override { return false; } // Conflicts with attack/defend
private:
    float m_gatherTimer = 0.0f;
};

class AttackBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
    BehaviorPriority getPriority() const override { return BehaviorPriority::Normal; } // Offensive action
    bool canRunConcurrently() const override { return false; } // Exclusive with defend/gather
private:
    float m_attackTimer = 0.0f;
};

class DefendBehavior : public AIBehavior {
public:
    void execute(Engine::Core::World* world, int playerId, float deltaTime) override;
    bool shouldExecute(Engine::Core::World* world, int playerId) const override;
    BehaviorPriority getPriority() const override { return BehaviorPriority::Critical; } // Survival first
    bool canRunConcurrently() const override { return false; } // Exclusive with attack/gather
private:
    float m_defendTimer = 0.0f;
};

} // namespace Game::Systems
