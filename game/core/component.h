#pragma once

#include "../units/troop_type.h"
#include "entity.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::Core {

class TransformComponent : public Component {
public:
  TransformComponent(float x = 0.0f, float y = 0.0f, float z = 0.0f,
                     float rotX = 0.0f, float rotY = 0.0f, float rotZ = 0.0f,
                     float scaleX = 1.0f, float scaleY = 1.0f,
                     float scaleZ = 1.0f)
      : position{x, y, z}, rotation{rotX, rotY, rotZ},
        scale{scaleX, scaleY, scaleZ} {}

  struct Vec3 {
    float x, y, z;
  };
  Vec3 position;
  Vec3 rotation;
  Vec3 scale;

  float desiredYaw = 0.0f;
  bool hasDesiredYaw = false;
};

class RenderableComponent : public Component {
public:
  enum class MeshKind { None, Quad, Plane, Cube, Capsule, Ring };

  RenderableComponent(const std::string &meshPath,
                      const std::string &texturePath)
      : meshPath(meshPath), texturePath(texturePath), visible(true),
        mesh(MeshKind::Cube) {
    color[0] = color[1] = color[2] = 1.0f;
  }

  std::string meshPath;
  std::string texturePath;
  bool visible;
  MeshKind mesh;
  float color[3];
};

class UnitComponent : public Component {
public:
  UnitComponent(int health = 100, int maxHealth = 100, float speed = 1.0f,
                float vision = 12.0f)
      : health(health), maxHealth(maxHealth), speed(speed), ownerId(0),
        visionRange(vision) {}

  int health;
  int maxHealth;
  float speed;
  std::string unitType;
  int ownerId;
  float visionRange;
};

class MovementComponent : public Component {
public:
  MovementComponent()
      : hasTarget(false), targetX(0.0f), targetY(0.0f), goalX(0.0f),
        goalY(0.0f), vx(0.0f), vz(0.0f), pathPending(false),
        pendingRequestId(0), repathCooldown(0.0f), lastGoalX(0.0f),
        lastGoalY(0.0f), timeSinceLastPathRequest(0.0f) {}

  bool hasTarget;
  float targetX, targetY;
  float goalX, goalY;
  float vx, vz;
  std::vector<std::pair<float, float>> path;
  bool pathPending;
  std::uint64_t pendingRequestId;
  float repathCooldown;

  float lastGoalX, lastGoalY;
  float timeSinceLastPathRequest;
};

class AttackComponent : public Component {
public:
  enum class CombatMode { Ranged, Melee, Auto };

  AttackComponent(float range = 2.0f, int damage = 10, float cooldown = 1.0f)
      : range(range), damage(damage), cooldown(cooldown), timeSinceLast(0.0f),
        meleeRange(1.5f), meleeDamage(damage), meleeCooldown(cooldown),
        preferredMode(CombatMode::Auto), currentMode(CombatMode::Ranged),
        canMelee(true), canRanged(false), maxHeightDifference(2.0f),
        inMeleeLock(false), meleeLockTargetId(0) {}

  float range;
  int damage;
  float cooldown;
  float timeSinceLast;

  float meleeRange;
  int meleeDamage;
  float meleeCooldown;

  CombatMode preferredMode;
  CombatMode currentMode;

  bool canMelee;
  bool canRanged;

  float maxHeightDifference;

  bool inMeleeLock;
  EntityID meleeLockTargetId;

  bool isInMeleeRange(float distance, float heightDiff) const {
    return distance <= meleeRange && heightDiff <= maxHeightDifference;
  }

  bool isInRangedRange(float distance) const {
    return distance <= range && distance > meleeRange;
  }

  int getCurrentDamage() const {
    return (currentMode == CombatMode::Melee) ? meleeDamage : damage;
  }

  float getCurrentCooldown() const {
    return (currentMode == CombatMode::Melee) ? meleeCooldown : cooldown;
  }

  float getCurrentRange() const {
    return (currentMode == CombatMode::Melee) ? meleeRange : range;
  }
};

class AttackTargetComponent : public Component {
public:
  AttackTargetComponent() : targetId(0), shouldChase(false) {}

  EntityID targetId;
  bool shouldChase;
};

class PatrolComponent : public Component {
public:
  PatrolComponent() : currentWaypoint(0), patrolling(false) {}

  std::vector<std::pair<float, float>> waypoints;
  size_t currentWaypoint;
  bool patrolling;
};

} // namespace Engine::Core

namespace Engine::Core {

class BuildingComponent : public Component {
public:
  BuildingComponent() = default;
};

class ProductionComponent : public Component {
public:
  ProductionComponent()
      : inProgress(false), buildTime(4.0f), timeRemaining(0.0f),
        producedCount(0), maxUnits(5),
        productType(Game::Units::TroopType::Archer), rallyX(0.0f), rallyZ(0.0f),
        rallySet(false), villagerCost(1) {}

  bool inProgress;
  float buildTime;
  float timeRemaining;
  int producedCount;
  int maxUnits;
  Game::Units::TroopType productType;
  float rallyX, rallyZ;
  bool rallySet;
  int villagerCost;
  std::vector<Game::Units::TroopType> productionQueue;
};

class AIControlledComponent : public Component {
public:
  AIControlledComponent() = default;
};

class CaptureComponent : public Component {
public:
  CaptureComponent()
      : capturingPlayerId(-1), captureProgress(0.0f), requiredTime(15.0f),
        isBeingCaptured(false) {}

  int capturingPlayerId;
  float captureProgress;
  float requiredTime;
  bool isBeingCaptured;
};

class PendingRemovalComponent : public Component {
public:
  PendingRemovalComponent() = default;
};

class HoldModeComponent : public Component {
public:
  HoldModeComponent()
      : active(true), exitCooldown(0.0f), standUpDuration(2.0f) {}

  bool active;
  float exitCooldown;
  float standUpDuration;
};

} // namespace Engine::Core
