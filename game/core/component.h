#pragma once

#include "entity.h"
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
  UnitComponent(int health = 100, int maxHealth = 100, float speed = 1.0f)
      : health(health), maxHealth(maxHealth), speed(speed), ownerId(0) {}

  int health;
  int maxHealth;
  float speed;
  std::string unitType;
  int ownerId;
};

class MovementComponent : public Component {
public:
  MovementComponent()
      : hasTarget(false), targetX(0.0f), targetY(0.0f), vx(0.0f), vz(0.0f) {}

  bool hasTarget;
  float targetX, targetY;
  float vx, vz;
  std::vector<std::pair<float, float>> path;
};

class AttackComponent : public Component {
public:
  AttackComponent(float range = 2.0f, int damage = 10, float cooldown = 1.0f)
      : range(range), damage(damage), cooldown(cooldown), timeSinceLast(0.0f) {}

  float range;
  int damage;
  float cooldown;
  float timeSinceLast;
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
        producedCount(0), maxUnits(5), productType("archer"), rallyX(0.0f),
        rallyZ(0.0f), rallySet(false) {}

  bool inProgress;
  float buildTime;
  float timeRemaining;
  int producedCount;
  int maxUnits;
  std::string productType;
  float rallyX, rallyZ;
  bool rallySet;
};

} // namespace Engine::Core
