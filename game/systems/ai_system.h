#pragma once

#include "../core/system.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Engine {
namespace Core {
class World;
class Entity;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine

namespace Game::Systems {

enum class AIState {
  Idle,
  Gathering,
  Attacking,
  Defending,
  Retreating,
  Expanding
};

enum class BehaviorPriority {
  VeryLow = 0,
  Low = 1,
  Normal = 2,
  High = 3,
  Critical = 4
};

class AIBehavior {
public:
  virtual ~AIBehavior() = default;
  virtual void execute(Engine::Core::World *world, int playerId,
                       float deltaTime) = 0;
  virtual bool shouldExecute(Engine::Core::World *world,
                             int playerId) const = 0;
  virtual BehaviorPriority getPriority() const = 0;
  virtual bool canRunConcurrently() const { return false; }
};

struct AIContext {
  int playerId = 2;
  AIState state = AIState::Idle;
  float stateTimer = 0.0f;
  float decisionTimer = 0.0f;

  std::vector<Engine::Core::EntityID> militaryUnits;
  std::vector<Engine::Core::EntityID> buildings;
  Engine::Core::EntityID primaryBarracks = 0;

  float rallyX = 0.0f;
  float rallyZ = 0.0f;
  int targetPriority = 0;

  int totalUnits = 0;
  int idleUnits = 0;
  int combatUnits = 0;
  float averageHealth = 1.0f;

  std::mutex contextMutex;
  std::atomic<bool> isProcessing{false};
};

class AISystem : public Engine::Core::System {
public:
  AISystem();
  ~AISystem() override;

  void update(Engine::Core::World *world, float deltaTime) override;

  void registerBehavior(std::unique_ptr<AIBehavior> behavior);

  void setThreadedMode(bool enabled) { m_useThreading = enabled; }

private:
  void updateContext(Engine::Core::World *world, AIContext &ctx);
  void updateStateMachine(Engine::Core::World *world, AIContext &ctx,
                          float deltaTime);
  void executeBehaviors(Engine::Core::World *world, float deltaTime);
  void executeBehaviorsThreaded(Engine::Core::World *world, float deltaTime);

  AIContext m_enemyAI;
  std::vector<std::unique_ptr<AIBehavior>> m_behaviors;
  float m_globalUpdateTimer = 0.0f;
  bool m_useThreading = false;

  std::unique_ptr<std::thread> m_aiThread;
  std::condition_variable m_aiCondition;
  std::mutex m_aiMutex;
  std::atomic<bool> m_shouldStop{false};
  std::atomic<bool> m_hasWork{false};
};

class ProductionBehavior : public AIBehavior {
public:
  void execute(Engine::Core::World *world, int playerId,
               float deltaTime) override;
  bool shouldExecute(Engine::Core::World *world, int playerId) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::High;
  }
  bool canRunConcurrently() const override { return true; }

private:
  float m_productionTimer = 0.0f;
};

class GatherBehavior : public AIBehavior {
public:
  void execute(Engine::Core::World *world, int playerId,
               float deltaTime) override;
  bool shouldExecute(Engine::Core::World *world, int playerId) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Low;
  }
  bool canRunConcurrently() const override { return false; }

private:
  float m_gatherTimer = 0.0f;
};

class AttackBehavior : public AIBehavior {
public:
  void execute(Engine::Core::World *world, int playerId,
               float deltaTime) override;
  bool shouldExecute(Engine::Core::World *world, int playerId) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Normal;
  }
  bool canRunConcurrently() const override { return false; }

private:
  float m_attackTimer = 0.0f;
};

class DefendBehavior : public AIBehavior {
public:
  void execute(Engine::Core::World *world, int playerId,
               float deltaTime) override;
  bool shouldExecute(Engine::Core::World *world, int playerId) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Critical;
  }
  bool canRunConcurrently() const override { return false; }

private:
  float m_defendTimer = 0.0f;
};

} // namespace Game::Systems
