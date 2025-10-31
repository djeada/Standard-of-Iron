#pragma once

#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "entity.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Game::Systems {
enum class NationID : std::uint8_t;
}

namespace Engine::Core {

namespace Defaults {
inline constexpr int kUnitDefaultHealth = 100;
inline constexpr float kUnitDefaultVisionRange = 12.0F;

inline constexpr float kAttackDefaultRange = 2.0F;
inline constexpr int kAttackDefaultDamage = 10;
inline constexpr float kAttackMeleeRange = 1.5F;
inline constexpr float kAttackHeightTolerance = 2.0F;

inline constexpr float kProductionDefaultBuildTime = 4.0F;
inline constexpr int kProductionMaxUnits = 5;

inline constexpr float kCaptureRequiredTime = 15.0F;

inline constexpr float kHoldStandUpDuration = 2.0F;
} // namespace Defaults

class TransformComponent : public Component {
public:
  TransformComponent(float x = 0.0F, float y = 0.0F, float z = 0.0F,
                     float rotX = 0.0F, float rotY = 0.0F, float rotZ = 0.0F,
                     float scale_x = 1.0F, float scaleY = 1.0F,
                     float scale_z = 1.0F)
      : position{x, y, z}, rotation{rotX, rotY, rotZ},
        scale{scale_x, scaleY, scale_z} {}

  struct Vec3 {
    float x, y, z;
  };
  Vec3 position;
  Vec3 rotation;
  Vec3 scale;

  float desiredYaw = 0.0F;
  bool hasDesiredYaw = false;
};

class RenderableComponent : public Component {
public:
  enum class MeshKind { None, Quad, Plane, Cube, Capsule, Ring };

  RenderableComponent(std::string meshPath, std::string texturePath)
      : meshPath(std::move(meshPath)), texturePath(std::move(texturePath)) {
    color.fill(1.0F);
  }

  std::string meshPath;
  std::string texturePath;
  std::string rendererId;
  bool visible{true};
  MeshKind mesh{MeshKind::Cube};
  std::array<float, 3> color{};
};

class UnitComponent : public Component {
public:
  UnitComponent(int health = Defaults::kUnitDefaultHealth,
                int max_health = Defaults::kUnitDefaultHealth,
                float speed = 1.0F,
                float vision = Defaults::kUnitDefaultVisionRange)
      : health(health), max_health(max_health), speed(speed),
        vision_range(vision) {}

  int health;
  int max_health;
  float speed;
  Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
  int owner_id{0};
  float vision_range;
  Game::Systems::NationID nation_id{Game::Systems::NationID::KingdomOfIron};
};

class MovementComponent : public Component {
public:
  MovementComponent() = default;

  bool hasTarget{false};
  float target_x{0.0F}, target_y{0.0F};
  float goalX{0.0F}, goalY{0.0F};
  float vx{0.0F}, vz{0.0F};
  std::vector<std::pair<float, float>> path;
  bool pathPending{false};
  std::uint64_t pendingRequestId{0};
  float repathCooldown{0.0F};

  float lastGoalX{0.0F}, lastGoalY{0.0F};
  float timeSinceLastPathRequest{0.0F};
};

class AttackComponent : public Component {
public:
  enum class CombatMode { Ranged, Melee, Auto };

  AttackComponent(float range = Defaults::kAttackDefaultRange,
                  int damage = Defaults::kAttackDefaultDamage,
                  float cooldown = 1.0F)
      : range(range), damage(damage), cooldown(cooldown),
        meleeRange(Defaults::kAttackMeleeRange), meleeDamage(damage),
        meleeCooldown(cooldown),
        max_heightDifference(Defaults::kAttackHeightTolerance) {}

  float range;
  int damage;
  float cooldown;
  float timeSinceLast{0.0F};

  float meleeRange;
  int meleeDamage;
  float meleeCooldown;

  CombatMode preferredMode{CombatMode::Auto};
  CombatMode currentMode{CombatMode::Ranged};

  bool canMelee{true};
  bool canRanged{false};

  float max_heightDifference;

  bool inMeleeLock{false};
  EntityID meleeLockTargetId{0};

  [[nodiscard]] auto isInMeleeRange(float distance,
                                    float height_diff) const -> bool {
    return distance <= meleeRange && height_diff <= max_heightDifference;
  }

  [[nodiscard]] auto isInRangedRange(float distance) const -> bool {
    return distance <= range && distance > meleeRange;
  }

  [[nodiscard]] auto getCurrentDamage() const -> int {
    return (currentMode == CombatMode::Melee) ? meleeDamage : damage;
  }

  [[nodiscard]] auto getCurrentCooldown() const -> float {
    return (currentMode == CombatMode::Melee) ? meleeCooldown : cooldown;
  }

  [[nodiscard]] auto getCurrentRange() const -> float {
    return (currentMode == CombatMode::Melee) ? meleeRange : range;
  }
};

class AttackTargetComponent : public Component {
public:
  AttackTargetComponent() = default;

  EntityID target_id{0};
  bool shouldChase{false};
};

class PatrolComponent : public Component {
public:
  PatrolComponent() = default;

  std::vector<std::pair<float, float>> waypoints;
  size_t currentWaypoint{0};
  bool patrolling{false};
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
      : buildTime(Defaults::kProductionDefaultBuildTime),

        maxUnits(Defaults::kProductionMaxUnits) {}

  bool inProgress{false};
  float buildTime;
  float timeRemaining{0.0F};
  int producedCount{0};
  int maxUnits;
  Game::Units::TroopType product_type{Game::Units::TroopType::Archer};
  float rallyX{0.0F}, rallyZ{0.0F};
  bool rallySet{false};
  int villagerCost{1};
  std::vector<Game::Units::TroopType> productionQueue;
};

class AIControlledComponent : public Component {
public:
  AIControlledComponent() = default;
};

class CaptureComponent : public Component {
public:
  CaptureComponent() : requiredTime(Defaults::kCaptureRequiredTime) {}

  int capturing_player_id{-1};
  float captureProgress{0.0F};
  float requiredTime;
  bool isBeingCaptured{false};
};

class PendingRemovalComponent : public Component {
public:
  PendingRemovalComponent() = default;
};

class HoldModeComponent : public Component {
public:
  HoldModeComponent() : standUpDuration(Defaults::kHoldStandUpDuration) {}

  bool active{true};
  float exitCooldown{0.0F};
  float standUpDuration;
};

} // namespace Engine::Core
