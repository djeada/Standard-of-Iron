#pragma once

#include "../core/system.h"
#include <QVector3D>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
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

struct AIContext {
  int playerId = 0;
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
  bool barracksUnderThreat = false;
  int nearbyThreatCount = 0;
  float closestThreatDistance = 0.0f;
  QVector3D basePosition;
};

struct MovementSnapshot {
  bool hasComponent = false;
  bool hasTarget = false;
};

struct ProductionSnapshot {
  bool hasComponent = false;
  bool inProgress = false;
  float buildTime = 0.0f;
  float timeRemaining = 0.0f;
  int producedCount = 0;
  int maxUnits = 0;
  std::string productType;
  bool rallySet = false;
  float rallyX = 0.0f;
  float rallyZ = 0.0f;
};

struct EntitySnapshot {
  Engine::Core::EntityID id = 0;
  std::string unitType;
  int ownerId = 0;
  int health = 0;
  int maxHealth = 0;
  bool isBuilding = false;
  QVector3D position;
  MovementSnapshot movement;
  ProductionSnapshot production;
};

struct ContactSnapshot {
  Engine::Core::EntityID id = 0;
  bool isBuilding = false;
  QVector3D position;
};

struct AISnapshot {
  int playerId = 0;
  std::vector<EntitySnapshot> friendlies;
  std::vector<ContactSnapshot> visibleEnemies;
};

enum class AICommandType { MoveUnits, AttackTarget, StartProduction };

struct AICommand {
  AICommandType type = AICommandType::MoveUnits;
  std::vector<Engine::Core::EntityID> units;
  std::vector<QVector3D> moveTargets;
  Engine::Core::EntityID targetId = 0;
  bool shouldChase = false;
  Engine::Core::EntityID buildingId = 0;
  std::string productType;
};

class AIBehavior {
public:
  virtual ~AIBehavior() = default;
  virtual void execute(const AISnapshot &snapshot, AIContext &context,
                       float deltaTime,
                       std::vector<AICommand> &outCommands) = 0;
  virtual bool shouldExecute(const AISnapshot &snapshot,
                             const AIContext &context) const = 0;
  virtual BehaviorPriority getPriority() const = 0;
  virtual bool canRunConcurrently() const { return false; }
};

class AISystem : public Engine::Core::System {
public:
  AISystem();
  ~AISystem() override;

  void update(Engine::Core::World *world, float deltaTime) override;

  void registerBehavior(std::unique_ptr<AIBehavior> behavior);

private:
  struct AIJob {
    AISnapshot snapshot;
    AIContext context;
    float deltaTime = 0.0f;
  };

  struct AIResult {
    AIContext context;
    std::vector<AICommand> commands;
  };

  AISnapshot buildSnapshot(Engine::Core::World *world) const;
  void updateContext(const AISnapshot &snapshot, AIContext &ctx);
  void updateStateMachine(AIContext &ctx, float deltaTime);
  void executeBehaviors(const AISnapshot &snapshot, AIContext &ctx,
                        float deltaTime, std::vector<AICommand> &outCommands);
  void workerLoop();
  void processResults(Engine::Core::World *world);
  void applyCommands(Engine::Core::World *world,
                     const std::vector<AICommand> &commands);

  AIContext m_enemyAI;
  std::vector<std::unique_ptr<AIBehavior>> m_behaviors;
  float m_globalUpdateTimer = 0.0f;
  std::thread m_aiThread;
  std::mutex m_jobMutex;
  std::condition_variable m_jobCondition;
  bool m_hasPendingJob = false;
  AIJob m_pendingJob;
  std::mutex m_resultMutex;
  std::queue<AIResult> m_results;
  std::atomic<bool> m_shouldStop{false};
  std::atomic<bool> m_workerBusy{false};
};

class ProductionBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;
  bool shouldExecute(const AISnapshot &snapshot,
                     const AIContext &context) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::High;
  }
  bool canRunConcurrently() const override { return true; }

private:
  float m_productionTimer = 0.0f;
};

class GatherBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;
  bool shouldExecute(const AISnapshot &snapshot,
                     const AIContext &context) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Low;
  }
  bool canRunConcurrently() const override { return false; }

private:
  float m_gatherTimer = 0.0f;
};

class AttackBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;
  bool shouldExecute(const AISnapshot &snapshot,
                     const AIContext &context) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Normal;
  }
  bool canRunConcurrently() const override { return false; }

private:
  float m_attackTimer = 0.0f;
};

class DefendBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;
  bool shouldExecute(const AISnapshot &snapshot,
                     const AIContext &context) const override;
  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Critical;
  }
  bool canRunConcurrently() const override { return false; }

private:
  float m_defendTimer = 0.0f;
};

} // namespace Game::Systems
