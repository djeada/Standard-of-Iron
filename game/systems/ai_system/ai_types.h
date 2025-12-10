#pragma once

#include "../../units/spawn_type.h"
#include "../../units/troop_type.h"
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
  float buildTime = 0.0F;
  float timeRemaining = 0.0F;
  int producedCount = 0;
  int maxUnits = 0;
  Game::Units::TroopType product_type = Game::Units::TroopType::Archer;
  bool rallySet = false;
  float rallyX = 0.0F;
  float rallyZ = 0.0F;
  int queueSize = 0;
};

struct EntitySnapshot {
  Engine::Core::EntityID id = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
  int owner_id = 0;
  int health = 0;
  int max_health = 0;
  bool isBuilding = false;

  float posX = 0.0F;
  float posY = 0.0F;
  float posZ = 0.0F;

  MovementSnapshot movement;
  ProductionSnapshot production;
};

struct ContactSnapshot {
  Engine::Core::EntityID id = 0;
  bool isBuilding = false;

  float posX = 0.0F;
  float posY = 0.0F;
  float posZ = 0.0F;

  int health = 0;
  int max_health = 0;
  Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Archer;
};

struct AISnapshot {
  int player_id = 0;
  std::vector<EntitySnapshot> friendlies;
  std::vector<ContactSnapshot> visibleEnemies;

  float gameTime = 0.0F;
};

struct AIContext {
  int player_id = 0;
  AIState state = AIState::Idle;
  float stateTimer = 0.0F;
  float decisionTimer = 0.0F;

  const Game::Systems::Nation *nation = nullptr;

  std::vector<Engine::Core::EntityID> militaryUnits;
  std::vector<Engine::Core::EntityID> buildings;
  Engine::Core::EntityID primaryBarracks = 0;

  float rallyX = 0.0F;
  float rallyZ = 0.0F;
  int targetPriority = 0;

  int total_units = 0;
  int idleUnits = 0;
  int combatUnits = 0;
  float averageHealth = 1.0F;
  bool barracksUnderThreat = false;
  int nearbyThreatCount = 0;
  float closest_threatDistance = 0.0F;

  float basePosX = 0.0F;
  float basePosY = 0.0F;
  float basePosZ = 0.0F;

  struct UnitAssignment {
    BehaviorPriority ownerPriority = BehaviorPriority::Normal;
    float assignmentTime = 0.0F;
    std::string assignedTask;
  };
  std::unordered_map<Engine::Core::EntityID, UnitAssignment> assignedUnits;

  int meleeCount = 0;
  int rangedCount = 0;
  int damagedUnitsCount = 0;

  int visibleEnemyCount = 0;
  int enemyBuildingsCount = 0;
  float averageEnemyDistance = 0.0F;

  int max_troops_per_player = 50;

  std::unordered_map<Engine::Core::EntityID, float> buildingsUnderAttack;
};

struct AICommand {
  AICommandType type = AICommandType::MoveUnits;
  std::vector<Engine::Core::EntityID> units;

  std::vector<float> moveTargetX;
  std::vector<float> moveTargetY;
  std::vector<float> moveTargetZ;

  Engine::Core::EntityID target_id = 0;
  bool should_chase = false;
  Engine::Core::EntityID buildingId = 0;
  Game::Units::TroopType product_type = Game::Units::TroopType::Archer;
};

struct AIResult {
  AIContext context;
  std::vector<AICommand> commands;
};

struct AIJob {
  AISnapshot snapshot;
  AIContext context;
  float delta_time = 0.0F;
};

} // namespace Game::Systems::AI
