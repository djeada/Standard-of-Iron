#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Core {
using EntityID = unsigned int;
}

namespace Game::Systems {
struct Nation;
}

namespace Game::Systems::AI {

enum class AIState {
  Idle,
  Gathering,
  Attacking,
  Defending,
  Retreating,
  Expanding
};

enum class AICommandType { MoveUnits, AttackTarget, StartProduction };

enum class BehaviorPriority {
  VeryLow = 0,
  Low = 1,
  Normal = 2,
  High = 3,
  Critical = 4
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
  int queueSize = 0;
};

struct EntitySnapshot {
  Engine::Core::EntityID id = 0;
  std::string unitType;
  int ownerId = 0;
  int health = 0;
  int maxHealth = 0;
  bool isBuilding = false;

  float posX = 0.0f;
  float posY = 0.0f;
  float posZ = 0.0f;

  MovementSnapshot movement;
  ProductionSnapshot production;
};

struct ContactSnapshot {
  Engine::Core::EntityID id = 0;
  bool isBuilding = false;

  float posX = 0.0f;
  float posY = 0.0f;
  float posZ = 0.0f;

  int health = 0;
  int maxHealth = 0;
  std::string unitType;
};

struct AISnapshot {
  int playerId = 0;
  std::vector<EntitySnapshot> friendlies;
  std::vector<ContactSnapshot> visibleEnemies;

  float gameTime = 0.0f;
};

struct AIContext {
  int playerId = 0;
  AIState state = AIState::Idle;
  float stateTimer = 0.0f;
  float decisionTimer = 0.0f;

  const Game::Systems::Nation *nation = nullptr;

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

  float basePosX = 0.0f;
  float basePosY = 0.0f;
  float basePosZ = 0.0f;

  struct UnitAssignment {
    BehaviorPriority ownerPriority = BehaviorPriority::Normal;
    float assignmentTime = 0.0f;
    std::string assignedTask;
  };
  std::unordered_map<Engine::Core::EntityID, UnitAssignment> assignedUnits;

  int meleeCount = 0;
  int rangedCount = 0;
  int damagedUnitsCount = 0;

  int visibleEnemyCount = 0;
  int enemyBuildingsCount = 0;
  float averageEnemyDistance = 0.0f;

  int maxTroopsPerPlayer = 50;

  std::unordered_map<Engine::Core::EntityID, float> buildingsUnderAttack;
};

struct AICommand {
  AICommandType type = AICommandType::MoveUnits;
  std::vector<Engine::Core::EntityID> units;

  std::vector<float> moveTargetX;
  std::vector<float> moveTargetY;
  std::vector<float> moveTargetZ;

  Engine::Core::EntityID targetId = 0;
  bool shouldChase = false;
  Engine::Core::EntityID buildingId = 0;
  std::string productType;
};

struct AIResult {
  AIContext context;
  std::vector<AICommand> commands;
};

struct AIJob {
  AISnapshot snapshot;
  AIContext context;
  float deltaTime = 0.0f;
};

} // namespace Game::Systems::AI
