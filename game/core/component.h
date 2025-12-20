#pragma once

#include "../systems/nation_id.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "entity.h"
#include <algorithm>
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
inline constexpr int kProductionMaxUnits = 10000;

inline constexpr float kCaptureRequiredTime = 15.0F;

inline constexpr float kHoldStandUpDuration = 2.0F;

inline constexpr float kGuardDefaultRadius = 10.0F;
inline constexpr float kGuardReturnThreshold = 1.0F;
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
  std::size_t path_index{0};
  bool path_pending{false};
  std::uint64_t pending_request_id{0};
  float repath_cooldown{0.0F};

  float last_goal_x{0.0F}, last_goal_y{0.0F};
  float time_since_last_path_request{0.0F};

  // Deadlock detection and recovery
  float last_position_x{0.0F}, last_position_z{0.0F};
  float time_stuck{0.0F};
  float unstuck_cooldown{0.0F};

  void clear_path() {
    path.clear();
    path_index = 0;
  }

  [[nodiscard]] auto has_waypoints() const -> bool {
    return path_index < path.size();
  }

  [[nodiscard]] auto
  current_waypoint() const -> const std::pair<float, float> & {

    return path[path_index];
  }

  void advance_waypoint() {
    if (path_index < path.size()) {
      ++path_index;
    }
  }

  [[nodiscard]] auto remaining_waypoints() const -> std::size_t {
    return path.size() > path_index ? path.size() - path_index : 0;
  }

  void validate_path_index() {
    if (path_index > path.size()) {
      path_index = path.size();
    }
  }
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

enum class CombatAnimationState : std::uint8_t {
  Idle,
  Advance,
  WindUp,
  Strike,
  Impact,
  Recover,
  Reposition
};

class CombatStateComponent : public Component {
public:
  CombatStateComponent() = default;

  CombatAnimationState animation_state{CombatAnimationState::Idle};
  float state_time{0.0F};
  float state_duration{0.0F};
  float attack_offset{0.0F};
  std::uint8_t attack_variant{0};
  bool is_hit_paused{false};
  float hit_pause_remaining{0.0F};

  static constexpr float kHitPauseDuration = 0.05F;
  static constexpr float kAdvanceDuration = 0.12F;
  static constexpr float kWindUpDuration = 0.15F;
  static constexpr float kStrikeDuration = 0.20F;
  static constexpr float kImpactDuration = 0.08F;
  static constexpr float kRecoverDuration = 0.25F;
  static constexpr float kRepositionDuration = 0.15F;
  static constexpr std::uint8_t kMaxAttackVariants = 3;
};

class HitFeedbackComponent : public Component {
public:
  HitFeedbackComponent() = default;

  bool is_reacting{false};
  float reaction_time{0.0F};
  float reaction_intensity{0.0F};
  float knockback_x{0.0F};
  float knockback_z{0.0F};

  static constexpr float kReactionDuration = 0.25F;
  static constexpr float kMaxKnockback = 0.15F;
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

  Game::Systems::NationID original_nation_id{
      Game::Systems::NationID::RomanRepublic};
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

class BuilderProductionComponent : public Component {
public:
  BuilderProductionComponent() = default;

  bool in_progress{false};
  float build_time{10.0F};
  float time_remaining{0.0F};
  std::string product_type{};
  bool construction_complete{false};
  bool has_construction_site{false};
  float construction_site_x{0.0F};
  float construction_site_z{0.0F};
  bool at_construction_site{false};
  bool is_placement_preview{false};

  bool bypass_movement_active{false};
  float bypass_target_x{0.0F};
  float bypass_target_z{0.0F};
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

class GuardModeComponent : public Component {
public:
  GuardModeComponent() : guard_radius(Defaults::kGuardDefaultRadius) {}

  bool active{true};
  EntityID guarded_entity_id{0};
  float guard_position_x{0.0F};
  float guard_position_z{0.0F};
  float guard_radius;
  bool returning_to_guard_position{false};
  bool has_guard_target{false};
};

class HealerComponent : public Component {
public:
  HealerComponent() = default;

  float healing_range{8.0F};
  int healing_amount{5};
  float healing_cooldown{2.0F};
  float time_since_last_heal{0.0F};
  bool is_healing_active{false};
  float healing_target_x{0.0F};
  float healing_target_z{0.0F};
};

class CatapultLoadingComponent : public Component {
public:
  enum class LoadingState { Idle, Loading, ReadyToFire, Firing };

  CatapultLoadingComponent() = default;

  LoadingState state{LoadingState::Idle};
  float loading_time{0.0F};
  float loading_duration{2.0F};
  float firing_time{0.0F};
  float firing_duration{0.5F};

  EntityID target_id{0};
  float target_locked_x{0.0F};
  float target_locked_y{0.0F};
  float target_locked_z{0.0F};
  bool target_position_locked{false};

  [[nodiscard]] auto get_loading_progress() const -> float {
    if (loading_duration <= 0.0F) {
      return 1.0F;
    }
    return std::min(loading_time / loading_duration, 1.0F);
  }

  [[nodiscard]] auto get_firing_progress() const -> float {
    if (firing_duration <= 0.0F) {
      return 1.0F;
    }
    return std::min(firing_time / firing_duration, 1.0F);
  }

  [[nodiscard]] auto is_loading() const -> bool {
    return state == LoadingState::Loading;
  }

  [[nodiscard]] auto is_ready_to_fire() const -> bool {
    return state == LoadingState::ReadyToFire;
  }

  [[nodiscard]] auto is_firing() const -> bool {
    return state == LoadingState::Firing;
  }
};

class FormationModeComponent : public Component {
public:
  FormationModeComponent() = default;

  bool active{false};
  float formation_center_x{0.0F};
  float formation_center_z{0.0F};
};

class StaminaComponent : public Component {
public:
  static constexpr float kRunSpeedMultiplier = 1.5F;
  static constexpr float kMinStaminaToStartRun = 10.0F;
  static constexpr float kDefaultMaxStamina = 100.0F;
  static constexpr float kDefaultRegenRate = 10.0F;
  static constexpr float kDefaultDepletionRate = 20.0F;

  StaminaComponent() noexcept = default;

  float stamina{kDefaultMaxStamina};
  float max_stamina{kDefaultMaxStamina};
  float regen_rate{kDefaultRegenRate};
  float depletion_rate{kDefaultDepletionRate};
  bool is_running{false};
  bool run_requested{false};

  [[nodiscard]] auto get_stamina_ratio() const noexcept -> float {
    return max_stamina > 0.0F ? stamina / max_stamina : 0.0F;
  }

  [[nodiscard]] auto can_start_running() const noexcept -> bool {
    return stamina >= kMinStaminaToStartRun;
  }

  [[nodiscard]] auto has_stamina() const noexcept -> bool {
    return stamina > 0.0F;
  }

  void deplete(float delta_time) noexcept {
    stamina = std::max(0.0F, stamina - depletion_rate * delta_time);
  }

  void regenerate(float delta_time) noexcept {
    stamina = std::min(max_stamina, stamina + regen_rate * delta_time);
  }

  void initialize_from_stats(float new_max_stamina, float new_regen_rate,
                             float new_depletion_rate) noexcept {
    max_stamina = new_max_stamina;
    stamina = new_max_stamina;
    regen_rate = new_regen_rate;
    depletion_rate = new_depletion_rate;
  }
};

class HomeComponent : public Component {
public:
  HomeComponent() = default;

  int population_contribution{50};
  Engine::Core::EntityID nearest_barracks_id{0};
  float update_cooldown{0.0F};
};

} // namespace Engine::Core
