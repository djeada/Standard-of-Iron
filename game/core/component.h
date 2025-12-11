#pragma once

#include "../systems/nation_id.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "entity.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
                     float rot_x = 0.0F, float rot_y = 0.0F, float rot_z = 0.0F,
                     float scale_x = 1.0F, float scale_y = 1.0F,
                     float scale_z = 1.0F)
      : position{x, y, z}, rotation{rot_x, rot_y, rot_z},
        scale{scale_x, scale_y, scale_z} {}

  struct Vec3 {
    float x, y, z;
  };
  Vec3 position;
  Vec3 rotation;
  Vec3 scale;

  float desired_yaw = 0.0F;
  bool has_desired_yaw = false;
};

class RenderableComponent : public Component {
public:
  enum class MeshKind { None, Quad, Plane, Cube, Capsule, Ring };

  RenderableComponent(std::string mesh_path, std::string texture_path)
      : mesh_path(std::move(mesh_path)), texture_path(std::move(texture_path)) {
    color.fill(1.0F);
  }

  std::string mesh_path;
  std::string texture_path;
  std::string renderer_id;
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
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
};

class MovementComponent : public Component {
public:
  MovementComponent() = default;

  bool has_target{false};
  float target_x{0.0F}, target_y{0.0F};
  float goal_x{0.0F}, goal_y{0.0F};
  float vx{0.0F}, vz{0.0F};
  std::vector<std::pair<float, float>> path;
  bool path_pending{false};
  std::uint64_t pending_request_id{0};
  float repath_cooldown{0.0F};

  float last_goal_x{0.0F}, last_goal_y{0.0F};
  float time_since_last_path_request{0.0F};
};

class AttackComponent : public Component {
public:
  enum class CombatMode { Ranged, Melee, Auto };

  AttackComponent(float range = Defaults::kAttackDefaultRange,
                  int damage = Defaults::kAttackDefaultDamage,
                  float cooldown = 1.0F)
      : range(range), damage(damage), cooldown(cooldown),
        melee_range(Defaults::kAttackMeleeRange), melee_damage(damage),
        melee_cooldown(cooldown),
        max_height_difference(Defaults::kAttackHeightTolerance) {}

  float range;
  int damage;
  float cooldown;
  float time_since_last{0.0F};

  float melee_range;
  int melee_damage;
  float melee_cooldown;

  CombatMode preferred_mode{CombatMode::Auto};
  CombatMode current_mode{CombatMode::Ranged};

  bool can_melee{true};
  bool can_ranged{false};

  float max_height_difference;

  bool in_melee_lock{false};
  EntityID melee_lock_target_id{0};

  [[nodiscard]] auto is_in_melee_range(float distance,
                                       float height_diff) const -> bool {
    return distance <= melee_range && height_diff <= max_height_difference;
  }

  [[nodiscard]] auto is_in_ranged_range(float distance) const -> bool {
    return distance <= range && distance > melee_range;
  }

  [[nodiscard]] auto get_current_damage() const -> int {
    return (current_mode == CombatMode::Melee) ? melee_damage : damage;
  }

  [[nodiscard]] auto get_current_cooldown() const -> float {
    return (current_mode == CombatMode::Melee) ? melee_cooldown : cooldown;
  }

  [[nodiscard]] auto get_current_range() const -> float {
    return (current_mode == CombatMode::Melee) ? melee_range : range;
  }
};

class AttackTargetComponent : public Component {
public:
  AttackTargetComponent() = default;

  EntityID target_id{0};
  bool should_chase{false};
};

class PatrolComponent : public Component {
public:
  PatrolComponent() = default;

  std::vector<std::pair<float, float>> waypoints;
  size_t current_waypoint{0};
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
      : build_time(Defaults::kProductionDefaultBuildTime),

        max_units(Defaults::kProductionMaxUnits) {}

  bool in_progress{false};
  float build_time;
  float time_remaining{0.0F};
  int produced_count{0};
  int max_units;
  Game::Units::TroopType product_type{Game::Units::TroopType::Archer};
  float rally_x{0.0F}, rally_z{0.0F};
  bool rally_set{false};
  int villager_cost{1};
  std::vector<Game::Units::TroopType> production_queue;
};

class AIControlledComponent : public Component {
public:
  AIControlledComponent() = default;
};

class CaptureComponent : public Component {
public:
  CaptureComponent() : required_time(Defaults::kCaptureRequiredTime) {}

  int capturing_player_id{-1};
  float capture_progress{0.0F};
  float required_time;
  bool is_being_captured{false};
};

class PendingRemovalComponent : public Component {
public:
  PendingRemovalComponent() = default;
};

class HoldModeComponent : public Component {
public:
  HoldModeComponent() : stand_up_duration(Defaults::kHoldStandUpDuration) {}

  bool active{true};
  float exit_cooldown{0.0F};
  float stand_up_duration;
};

class HealerComponent : public Component {
public:
  HealerComponent() = default;

  float healing_range{8.0F};
  int healing_amount{5};
  float healing_cooldown{2.0F};
  float time_since_last_heal{0.0F};
};

} // namespace Engine::Core
