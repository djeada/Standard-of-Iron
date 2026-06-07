#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../systems/nation_id.h"
#include "../systems/projectile_kind.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "entity.h"

namespace Game::Systems {
class MovementSystem;
}

namespace Engine::Core {

namespace Defaults {
inline constexpr int k_unit_default_health = 100;
inline constexpr float k_unit_default_vision_range = 12.0F;

inline constexpr float k_attack_default_range = 2.0F;
inline constexpr int k_attack_default_damage = 10;
inline constexpr float k_attack_melee_range = 1.5F;
inline constexpr float k_attack_height_tolerance = 2.0F;

inline constexpr float k_production_default_build_time = 4.0F;
inline constexpr int k_production_max_units = 10000;

inline constexpr float k_capture_required_time = 15.0F;

inline constexpr float k_hold_stand_up_duration = 2.0F;
inline constexpr float k_hold_kneel_duration = 1.5F;

inline constexpr float k_guard_enter_duration = 1.8F;
inline constexpr float k_guard_exit_duration = 1.4F;

inline constexpr float k_guard_default_radius = 10.0F;
inline constexpr float k_guard_return_threshold = 1.0F;
inline constexpr float k_blood_stain_default_radius = 0.6F;
inline constexpr float k_blood_stain_default_aspect_ratio = 1.0F;
inline constexpr float k_blood_stain_default_lifetime = 8.0F;
inline constexpr int k_blood_stain_max_active = 10;
} // namespace Defaults

class TransformComponent : public Component {
public:
  TransformComponent(float x = 0.0F,
                     float y = 0.0F,
                     float z = 0.0F,
                     float rot_x = 0.0F,
                     float rot_y = 0.0F,
                     float rot_z = 0.0F,
                     float scale_x = 1.0F,
                     float scale_y = 1.0F,
                     float scale_z = 1.0F)
      : position{x, y, z}
      , rotation{rot_x, rot_y, rot_z}
      , scale{scale_x, scale_y, scale_z} {}

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
  enum class MeshKind {
    None,
    Quad,
    Plane,
    Cube,
    Capsule,
    Ring
  };

  RenderableComponent(std::string mesh_path, std::string texture_path)
      : mesh_path(std::move(mesh_path))
      , texture_path(std::move(texture_path)) {
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
  UnitComponent(int health = Defaults::k_unit_default_health,
                int max_health = Defaults::k_unit_default_health,
                float speed = 1.0F,
                float vision = Defaults::k_unit_default_vision_range)
      : health(health)
      , max_health(max_health)
      , speed(speed)
      , vision_range(vision) {}

  int health;
  int max_health;
  float speed;
  Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
  int owner_id{0};
  float vision_range;
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  int render_individuals_per_unit_override{0};
  bool render_rider{true};
  std::uint8_t death_sequence_override{0xFF};
};

[[nodiscard]] inline auto resolve_surviving_individual_count(
    int health, int max_health, int individuals_per_unit) noexcept -> int {
  if (individuals_per_unit <= 0 || health <= 0) {
    return 0;
  }
  int const safe_max_health = std::max(1, max_health);
  float const ratio =
      std::clamp(health / static_cast<float>(safe_max_health), 0.0F, 1.0F);
  return std::max(1,
                  std::min(individuals_per_unit,
                           static_cast<int>(std::ceil(
                               ratio * static_cast<float>(individuals_per_unit)))));
}

enum class MovementState : std::uint8_t {
  Idle = 0,
  FollowingPath = 2,
  FollowingDirect = 3
};

class MovementComponent : public Component {
public:
  MovementComponent() = default;

  [[nodiscard]] auto get_has_target() const -> bool { return has_target; }
  [[nodiscard]] auto get_target_x() const -> float { return target_x; }
  [[nodiscard]] auto get_target_y() const -> float { return target_y; }
  [[nodiscard]] auto get_goal_x() const -> float { return goal_x; }
  [[nodiscard]] auto get_goal_y() const -> float { return goal_y; }
  [[nodiscard]] auto get_vx() const -> float { return vx; }
  [[nodiscard]] auto get_vz() const -> float { return vz; }
  [[nodiscard]] auto get_path() const -> const std::vector<std::pair<float, float>>& {
    return path;
  }
  [[nodiscard]] auto get_path_index() const -> std::size_t { return path_index; }
  [[nodiscard]] auto get_state() const -> MovementState {
    if (has_target && has_waypoints()) {
      return MovementState::FollowingPath;
    }
    if (has_target) {
      return MovementState::FollowingDirect;
    }
    return MovementState::Idle;
  }

  void clear_path() {
    path.clear();
    path_index = 0;
  }

  void stop() {
    has_target = false;
    clear_path();
    vx = 0.0F;
    vz = 0.0F;
  }

  [[nodiscard]] auto has_waypoints() const -> bool { return path_index < path.size(); }

  [[nodiscard]] auto current_waypoint() const -> const std::pair<float, float>& {
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

  void set_rest_position(float x, float z) {
    goal_x = x;
    goal_y = z;
    target_x = x;
    target_y = z;
  }

  void engage_manual_move(float x, float z) {
    has_target = true;
    target_x = x;
    target_y = z;
    goal_x = x;
    goal_y = z;
  }

  void set_manual_velocity(float new_vx, float new_vz) {
    vx = new_vx;
    vz = new_vz;
  }

private:
  friend class Game::Systems::MovementSystem;
  friend class Serialization;
  friend struct MovementTestAccess;

  bool has_target{false};
  float target_x{0.0F}, target_y{0.0F};
  float goal_x{0.0F}, goal_y{0.0F};
  float vx{0.0F}, vz{0.0F};
  std::vector<std::pair<float, float>> path;
  std::size_t path_index{0};

  bool stuck_ref_valid{false};
  float stuck_ref_x{0.0F}, stuck_ref_z{0.0F};
  float stuck_timer{0.0F};
};

enum class PlayerOrderIntentKind : std::uint8_t {
  None,
  ManualMove
};

class PlayerOrderIntentComponent : public Component {
public:
  PlayerOrderIntentKind kind{PlayerOrderIntentKind::None};
  bool suppress_opportunistic_combat{false};
};

enum class MotionPresentationSource : std::uint8_t {
  None,
  Navigation,
  Chase,
  DirectControl,
  BuilderBypass,
  ForcedDisplacement
};

enum class MotionPresentationState : std::uint8_t {
  Idle,
  Walk,
  Run
};

class MotionPresentationComponent : public Component {
public:
  MotionPresentationComponent() = default;

  bool snapshot_valid{false};
  bool initialized{false};
  float previous_x{0.0F}, previous_y{0.0F}, previous_z{0.0F};
  float displacement_x{0.0F}, displacement_z{0.0F};
  float velocity_x{0.0F}, velocity_z{0.0F};
  float speed{0.0F};
  float direction_x{0.0F}, direction_z{1.0F};
  bool has_velocity{false};
  bool has_navigation_intent{false};
  bool has_chase_intent{false};
  bool attack_target_in_range{false};
  bool has_movement_target{false};
  float movement_target_x{0.0F}, movement_target_z{0.0F};
  MotionPresentationSource source{MotionPresentationSource::None};
  MotionPresentationState state{MotionPresentationState::Idle};
  MotionPresentationState previous_state{MotionPresentationState::Idle};
  bool state_changed{false};
  float state_time{0.0F};
  float seconds_since_motion{0.0F};
  float tick_delta_time{0.0F};

  void set_state(MotionPresentationState next_state) noexcept {
    previous_state = state;
    state = next_state;
    state_changed = state != previous_state;
  }

  [[nodiscard]] auto is_idle_state() const noexcept -> bool {
    return state == MotionPresentationState::Idle;
  }
  [[nodiscard]] auto is_walk_state() const noexcept -> bool {
    return state == MotionPresentationState::Walk;
  }
  [[nodiscard]] auto is_run_state() const noexcept -> bool {
    return state == MotionPresentationState::Run;
  }
  [[nodiscard]] auto has_locomotion() const noexcept -> bool {
    return state != MotionPresentationState::Idle;
  }
};

class AttackComponent : public Component {
public:
  enum class CombatMode {
    Ranged,
    Melee,
    Auto
  };

  AttackComponent(float range = Defaults::k_attack_default_range,
                  int damage = Defaults::k_attack_default_damage,
                  float cooldown = 1.0F)
      : range(range)
      , damage(damage)
      , cooldown(cooldown)
      , melee_range(Defaults::k_attack_melee_range)
      , melee_damage(damage)
      , melee_cooldown(cooldown)
      , max_height_difference(Defaults::k_attack_height_tolerance) {}

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

  bool has_pending_melee_strike{false};
  EntityID pending_melee_target_id{0};
  int pending_melee_damage{0};
  float pending_melee_elapsed{0.0F};
  float pending_melee_contact_time{0.0F};

  static constexpr float k_melee_contact_range_grace = 0.75F;

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
  bool is_player_command{false};
};

class RpgCommanderTargetComponent : public Component {
public:
  RpgCommanderTargetComponent() = default;

  EntityID explicit_lock_target_id{0};
  EntityID aim_candidate_id{0};
  EntityID recent_hit_target_id{0};
  float recent_hit_timer{0.0F};
};

enum class RpgCommanderActionPhase : std::uint8_t {
  None,
  Strike,
  Guard,
  Dodge,
  Jump,
  Ability
};

class RpgCommanderActionComponent : public Component {
public:
  RpgCommanderActionComponent() = default;

  RpgCommanderActionPhase phase{RpgCommanderActionPhase::None};
  std::uint8_t melee_attack_style{0};
  std::uint8_t melee_attack_sequence{0};
  EntityID active_target_id{0};
  EntityID last_hit_target_id{0};
  int last_damage{0};
  float phase_time{0.0F};
};

enum class CombatAttackFamily : std::uint8_t {
  None = 0,
  Sword = 1,
  Spear = 2,
  Bow = 3
};

[[nodiscard]] inline auto resolve_combat_attack_family(
    Game::Units::SpawnType spawn_type,
    AttackComponent::CombatMode mode) noexcept -> CombatAttackFamily {
  using Game::Units::SpawnType;
  if (mode == AttackComponent::CombatMode::Ranged) {
    switch (spawn_type) {
    case SpawnType::Archer:
    case SpawnType::SkeletonArcher:
    case SpawnType::GravePriest:
    case SpawnType::HorseArcher:
      return CombatAttackFamily::Bow;
    default:
      return CombatAttackFamily::None;
    }
  }

  switch (spawn_type) {
  case SpawnType::Knight:
  case SpawnType::MountedKnight:
  case SpawnType::Archer:
  case SpawnType::HorseArcher:
  case SpawnType::SkeletonSwordsman:
  case SpawnType::SkeletonArcher:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageMercenaryBroker:
  case SpawnType::CarthageCavalryPatron:
  case SpawnType::CarthageElephantMaster:
    return CombatAttackFamily::Sword;
  case SpawnType::Spearman:
  case SpawnType::HorseSpearman:
    return CombatAttackFamily::Spear;
  case SpawnType::GravePriest:
    return CombatAttackFamily::Sword;
  default:
    return CombatAttackFamily::None;
  }
}

enum class CombatAnimationState : std::uint8_t {
  Idle,
  Advance,
  WindUp,
  Strike,
  Impact,
  Recover,
  Reposition
};

enum class AttackDirection : std::uint8_t {
  LeftSlash = 0,
  RightSlash = 1,
  Overhead = 2,
  Thrust = 3,
  HeavyOverhead = 4
};

class CombatStateComponent : public Component {
public:
  CombatStateComponent() = default;

  CombatAnimationState animation_state{CombatAnimationState::Idle};
  CombatAttackFamily attack_family{CombatAttackFamily::None};
  AttackDirection attack_direction{AttackDirection::LeftSlash};
  float state_time{0.0F};
  float state_duration{0.0F};
  float attack_offset{0.0F};
  std::uint8_t attack_variant{0};
  bool finisher_attack{false};
  bool is_hit_paused{false};
  float hit_pause_remaining{0.0F};
  bool damage_dealt_this_swing{false};
  bool input_buffered{false};

  float swing_duration_scale{1.0F};

  static constexpr float k_combat_animation_hit_pause_duration = 0.10F;
  static constexpr float k_advance_duration = 0.22F;
  static constexpr float k_wind_up_duration = 0.42F;
  static constexpr float k_strike_duration = 0.34F;
  static constexpr float k_impact_duration = 0.18F;
  static constexpr float k_recover_duration = 0.40F;
  static constexpr float k_reposition_duration = 0.28F;

  static constexpr float k_base_cycle_total =
      k_advance_duration + k_wind_up_duration + k_strike_duration + k_impact_duration +
      k_recover_duration + k_reposition_duration;

  [[nodiscard]] static auto cooldown_fit_scale(float cooldown) -> float {
    if (cooldown <= 0.0F) {
      return 1.0F;
    }
    float scale = cooldown / k_base_cycle_total;
    if (scale < 0.15F) {
      scale = 0.15F;
    } else if (scale > 3.0F) {
      scale = 3.0F;
    }
    return scale;
  }

  static constexpr float k_melee_contact_fraction =
      (k_advance_duration + k_wind_up_duration) /
      (k_advance_duration + k_wind_up_duration + k_strike_duration + k_impact_duration +
       k_recover_duration + k_reposition_duration);

  static constexpr std::uint8_t k_attack_variant_seed_slots = 8;

  static constexpr float k_stamina_cost_light_attack = 12.0F;
  static constexpr float k_stamina_cost_heavy_attack = 25.0F;
  static constexpr float k_stamina_cost_dodge = 18.0F;
  static constexpr float k_stamina_cost_guard_per_second = 8.0F;
  static constexpr float k_stamina_cost_shield_bash = 20.0F;
  static constexpr float k_stamina_cost_jump = 10.0F;
  static constexpr float k_low_stamina_threshold = 20.0F;
  static constexpr float k_low_stamina_damage_penalty = 0.7F;
};

enum class StaggerTier : std::uint8_t {
  LightFlinch = 0,
  HeavyStagger = 1,
  Knockback = 2,
  Knockdown = 3,
  GuardBreak = 4
};

class HitFeedbackComponent : public Component {
public:
  HitFeedbackComponent() = default;

  bool is_reacting{false};
  float reaction_time{0.0F};
  float reaction_intensity{0.0F};
  float knockback_x{0.0F};
  float knockback_z{0.0F};
  StaggerTier stagger_tier{StaggerTier::LightFlinch};
  float hit_direction_x{0.0F};
  float hit_direction_z{0.0F};

  static constexpr float k_reaction_duration = 0.25F;
  static constexpr float k_max_knockback = 0.15F;
  static constexpr float k_light_flinch_duration = 0.15F;
  static constexpr float k_heavy_stagger_duration = 0.40F;
  static constexpr float k_knockback_duration = 0.55F;
  static constexpr float k_knockdown_duration = 0.90F;
  static constexpr float k_guard_break_duration = 0.65F;

  [[nodiscard]] static auto duration_for_tier(StaggerTier tier) noexcept -> float {
    switch (tier) {
    case StaggerTier::LightFlinch:
      return k_light_flinch_duration;
    case StaggerTier::HeavyStagger:
      return k_heavy_stagger_duration;
    case StaggerTier::Knockback:
      return k_knockback_duration;
    case StaggerTier::Knockdown:
      return k_knockdown_duration;
    case StaggerTier::GuardBreak:
      return k_guard_break_duration;
    }
    return k_reaction_duration;
  }
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

  Game::Systems::NationID original_nation_id{Game::Systems::NationID::RomanRepublic};
};

class ProductionComponent : public Component {
public:
  ProductionComponent()
      : build_time(Defaults::k_production_default_build_time)
      ,

      max_units(Defaults::k_production_max_units) {}

  bool in_progress{false};
  float build_time;
  float time_remaining{0.0F};
  int produced_count{0};
  int max_units;
  Game::Units::TroopType product_type{Game::Units::TroopType::Archer};
  float rally_x{0.0F}, rally_z{0.0F};
  bool rally_set{false};
  int villager_cost{1};
  int manpower_available{0};
  std::vector<Game::Units::TroopType> production_queue;
};

class MoraleComponent : public Component {
public:
  MoraleComponent() = default;

  float morale{70.0F};
  float commander_aura_bonus{0.0F};
  float shock_timer{0.0F};
  bool wavering{false};
  bool routing{false};
};

inline void refresh_morale_state(MoraleComponent& morale) noexcept {
  morale.morale = std::clamp(morale.morale, 0.0F, 100.0F);
  morale.routing = morale.morale < 20.0F;
  morale.wavering = !morale.routing && morale.morale < 40.0F;
}

class UndeadComponent : public Component {
public:
  UndeadComponent() = default;

  bool morale_immune{true};
  float fire_damage_multiplier{1.5F};
  float priest_damage_multiplier{1.4F};
  float cavalry_charge_damage_multiplier{1.25F};
  bool counts_for_economy{false};
};

class CursedStatusComponent : public Component {
public:
  CursedStatusComponent() = default;

  float morale_penalty_per_hit{8.0F};
  float duration{6.0F};
  float remaining_duration{6.0F};
  int stacks{1};
};

class BurningStatusComponent : public Component {
public:
  BurningStatusComponent() = default;

  float duration{2.5F};
  float remaining_duration{2.5F};
  float ignition_elapsed{0.0F};
  float tick_interval{0.5F};
  float tick_accumulator{0.0F};
  int damage_per_tick{2};
  EntityID attacker_id{0};
  float fire_bonus_multiplier{1.0F};
};

class CommanderComponent : public Component {
public:
  CommanderComponent() = default;

  void
  begin_flag_rally(float target_x, float target_z, bool already_at_position) noexcept {
    flag_rally_pending_x = target_x;
    flag_rally_pending_z = target_z;
    flag_rally_animation_timer = already_at_position ? flag_rally_cost : 0.0F;
    flag_rally_in_progress = true;
    flag_rally_at_position = already_at_position;
    flag_rally_flag_active = false;
    flag_rally_issue_commands = false;
  }

  void cancel_flag_rally() noexcept {
    flag_rally_animation_timer = 0.0F;
    flag_rally_in_progress = false;
    flag_rally_at_position = false;
    flag_rally_flag_active = false;
    flag_rally_issue_commands = false;
  }

  void complete_flag_rally() noexcept {
    flag_rally_flag_x = flag_rally_pending_x;
    flag_rally_flag_z = flag_rally_pending_z;
    flag_rally_flag_active = true;
    flag_rally_issue_commands = true;
    flag_rally_animation_timer = 0.0F;
    flag_rally_in_progress = false;
    flag_rally_at_position = false;
  }

  [[nodiscard]] auto is_flag_rally_planting() const noexcept -> bool {
    return flag_rally_in_progress && flag_rally_at_position;
  }

  std::string commander_id;
  std::string display_name;
  std::string strategic_identity;
  std::string passive_aura;
  std::string bonus_type;
  std::string bonus_summary;
  std::string rally_ability;
  std::string death_consequence;
  int bodyguard_count{6};
  float aura_radius{12.0F};
  float aura_morale_bonus{5.0F};
  float aura_bonus_value{0.0F};
  float rally_range{10.0F};
  float rally_cooldown{45.0F};
  float rally_morale_restore{25.0F};
  float rally_cooldown_remaining{0.0F};
  float rally_feedback_time{0.0F};
  bool rally_requested{false};
  bool rally_requires_manual_trigger{false};
  float death_shock_radius{14.0F};
  float death_morale_shock{25.0F};
  bool aura_active{true};

  bool aura_ability_active{false};
  bool aura_ability_requested{false};
  float aura_ability_duration{15.0F};
  float aura_ability_remaining{0.0F};
  float aura_ability_cooldown{60.0F};
  float aura_ability_cooldown_remaining{0.0F};
  Game::Units::SpawnType aura_affinity_spawn_type{Game::Units::SpawnType::Knight};

  bool wounded{false};
  bool fpv_controlled{false};
  int combo_step{0};
  bool power_strike_active{false};
  float shield_bash_cooldown_remaining{0.0F};
  float vanguard_rush_cooldown_remaining{0.0F};
  float second_wind_cooldown_remaining{0.0F};
  bool just_struck_enemy{false};
  std::uint8_t last_strike_combo_step{0U};
  bool jump_active{false};
  float jump_phase{0.0F};
  float jump_height_offset{0.0F};
  float fpv_motion_vx{0.0F};
  float fpv_motion_vz{0.0F};
  float posture{0.0F};
  float posture_max{100.0F};
  float punish_window_remaining{0.0F};
  bool close_camera_mode{false};

  float flag_rally_cost{3.0F};
  float flag_rally_pending_x{0.0F};
  float flag_rally_pending_z{0.0F};
  float flag_rally_animation_timer{0.0F};
  bool flag_rally_in_progress{false};
  bool flag_rally_at_position{false};
  float flag_rally_flag_x{0.0F};
  float flag_rally_flag_z{0.0F};
  bool flag_rally_flag_active{false};
  bool flag_rally_issue_commands{false};
};

class CommanderGuardComponent : public Component {
public:
  CommanderGuardComponent() = default;

  bool active{false};
  float frontal_arc_dot{0.15F};
  float damage_multiplier{0.45F};
  float perfect_guard_remaining{0.0F};
  float guard_break_remaining{0.0F};
  bool rearm_requires_release{false};
};

class RpgHealthComponent : public Component {
public:
  RpgHealthComponent() = default;

  int rpg_hp{150};
  int rpg_max_hp{150};
  float armor{0.0F};
  float crit_chance{0.05F};
  float crit_multiplier{1.8F};
  bool active{false};
  bool dodge_invincible{false};
};

class StaggerComponent : public Component {
public:
  explicit StaggerComponent(float duration = 0.5F)
      : remaining(duration) {}
  float remaining;
  StaggerTier tier{StaggerTier::LightFlinch};
};

enum class EnemyTelegraphPhase : std::uint8_t {
  None,
  WindUp,
  Active,
  Recovery
};

class EnemyTelegraphComponent : public Component {
public:
  EnemyTelegraphComponent() = default;

  EnemyTelegraphPhase phase{EnemyTelegraphPhase::None};
  float phase_time{0.0F};
  float wind_up_duration{0.45F};
  float active_duration{0.20F};
  float recovery_duration{0.35F};
  AttackDirection attack_direction{AttackDirection::LeftSlash};
  bool is_unblockable{false};
  bool visual_tell_active{false};
};

enum class RpgEngagementRole : std::uint8_t {
  Support,
  FrontAttacker,
  LeftThreat,
  RightThreat
};

class RpgEngagementComponent : public Component {
public:
  struct Slot {
    EntityID entity_id{0};
    RpgEngagementRole role{RpgEngagementRole::Support};
    float distance{0.0F};
    float signed_angle_degrees{0.0F};
  };

  std::vector<Slot> engagement_slots;
  EntityID front_attacker_id{0};
  EntityID left_threat_id{0};
  EntityID right_threat_id{0};
  int active_attackers{0};
  float ring_radius{5.0F};
};

class AIControlledComponent : public Component {
public:
  AIControlledComponent() = default;
};

class CaptureComponent : public Component {
public:
  CaptureComponent()
      : required_time(Defaults::k_capture_required_time) {}

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
  float construction_site_rotation_y{0.0F};
  bool has_task_target{false};
  std::uint64_t task_target_id{0};
  float task_target_x{0.0F};
  float task_target_z{0.0F};
  bool task_target_reserved{false};
  bool at_construction_site{false};
  bool is_placement_preview{false};
  EntityID construction_site_entity_id{0};
  std::vector<EntityID> queued_construction_site_ids{};

  bool bypass_movement_active{false};
  float bypass_target_x{0.0F};
  float bypass_target_z{0.0F};
};

class WallSegmentComponent : public Component {
public:
  WallSegmentComponent() = default;

  int grid_x{0};
  int grid_z{0};
  std::uint8_t connection_mask{0};
};

class WallConstructionSiteComponent : public Component {
public:
  WallConstructionSiteComponent() = default;

  int owner_id{0};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  float build_time{0.0F};
  float progress{0.0F};
};

class ConstructionPreviewComponent : public Component {
public:
  ConstructionPreviewComponent() = default;

  int owner_id{0};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  int grid_x{0};
  int grid_z{0};
  bool valid{false};
};

class PendingRemovalComponent : public Component {
public:
  PendingRemovalComponent() = default;
};

class BloodStainComponent : public Component {
public:
  BloodStainComponent(float radius = Defaults::k_blood_stain_default_radius,
                      float lifetime = Defaults::k_blood_stain_default_lifetime,
                      float rotation = 0.0F,
                      float aspect_ratio = Defaults::k_blood_stain_default_aspect_ratio,
                      float seed = 0.0F)
      : radius(radius)
      , lifetime(lifetime)
      , rotation(rotation)
      , aspect_ratio(aspect_ratio)
      , seed(seed) {}

  float radius;
  float elapsed_time{0.0F};
  float lifetime;
  float rotation{0.0F};
  float aspect_ratio{Defaults::k_blood_stain_default_aspect_ratio};
  float seed{0.0F};
};

class FirePatchComponent : public Component {
public:
  FirePatchComponent() = default;

  float radius{1.8F};
  float duration{4.0F};
  float remaining_duration{4.0F};
  float burn_duration{1.5F};
  float burn_tick_interval{0.5F};
  int burn_damage_per_tick{2};
  int attacker_owner_id{0};
  EntityID attacker_id{0};
  bool friendly_fire{false};
  float fire_bonus_multiplier{1.0F};
};

enum class DeathSequenceProfile : std::uint8_t {
  Infantry = 0,
  MountedRider = 1,
  Horse = 2,
  Elephant = 3
};

enum class DeathSequenceState : std::uint8_t {
  Dying = 0,
  DeadHold = 1
};

class DeathAnimationComponent : public Component {
public:
  DeathAnimationComponent() = default;

  DeathSequenceProfile profile{DeathSequenceProfile::Infantry};
  DeathSequenceState state{DeathSequenceState::Dying};
  float state_time{0.0F};
  float state_duration{1.0F};
  float dead_hold_duration{0.8F};
  std::uint8_t sequence_variant{0};
};

class SoldierCasualtyAnimationComponent : public Component {
public:
  struct Entry {
    std::uint16_t slot_index{0};
    DeathSequenceProfile profile{DeathSequenceProfile::Infantry};
    DeathSequenceState state{DeathSequenceState::Dying};
    float state_time{0.0F};
    float state_duration{1.0F};
    float dead_hold_duration{0.8F};
    std::uint8_t sequence_variant{0};
  };

  std::vector<Entry> entries;
};

class HoldModeComponent : public Component {
public:
  HoldModeComponent()
      : stand_up_duration(Defaults::k_hold_stand_up_duration)
      , kneel_duration(Defaults::k_hold_kneel_duration) {}

  void begin_exit() {
    active = false;
    exit_cooldown = stand_up_duration * std::clamp(kneel_entry_progress, 0.0F, 1.0F);
  }

  bool active{true};
  float exit_cooldown{0.0F};
  float stand_up_duration;
  float kneel_entry_progress{0.0F};
  float kneel_duration;
};

class GuardModeComponent : public Component {
public:
  GuardModeComponent()
      : guard_radius(Defaults::k_guard_default_radius) {}

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
  enum class TargetAffinity : std::uint8_t {
    LivingAllies = 0,
    UndeadAllies
  };

  HealerComponent() = default;

  float healing_range{8.0F};
  int healing_amount{5};
  float healing_cooldown{2.0F};
  float time_since_last_heal{0.0F};
  bool is_healing_active{false};
  float healing_target_x{0.0F};
  float healing_target_z{0.0F};
  TargetAffinity target_affinity{TargetAffinity::LivingAllies};
  bool suppress_attack_while_healing{true};
};

class SpecialAttackComponent : public Component {
public:
  SpecialAttackComponent() = default;

  Game::Systems::ProjectileKind projectile_kind = Game::Systems::ProjectileKind::Arrow;
  bool use_projectile_system{false};
  bool friendly_fire{false};
  float splash_radius{0.0F};
  float splash_damage_multiplier{0.6F};
  float bonus_damage_multiplier_vs_fire_vulnerable{1.0F};
  float cursed_duration{0.0F};
  float cursed_morale_penalty_per_hit{0.0F};
  int cursed_stacks_per_hit{0};
  float fire_patch_duration{0.0F};
  float fire_patch_radius{0.0F};
  float burn_duration{0.0F};
  float burn_tick_interval{0.5F};
  int burn_damage_per_tick{0};
};

class CatapultLoadingComponent : public Component {
public:
  enum class LoadingState {
    Idle,
    Loading,
    ReadyToFire,
    Firing
  };

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

  [[nodiscard]] auto is_firing() const -> bool { return state == LoadingState::Firing; }
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
  static constexpr float k_run_speed_multiplier = 1.5F;
  static constexpr float k_min_stamina_to_start_run = 10.0F;
  static constexpr float k_default_max_stamina = 100.0F;
  static constexpr float k_default_regen_rate = 10.0F;
  static constexpr float k_default_depletion_rate = 20.0F;

  StaminaComponent() noexcept = default;

  float stamina{k_default_max_stamina};
  float max_stamina{k_default_max_stamina};
  float regen_rate{k_default_regen_rate};
  float depletion_rate{k_default_depletion_rate};
  bool is_running{false};
  bool run_requested{false};

  [[nodiscard]] auto get_stamina_ratio() const noexcept -> float {
    return max_stamina > 0.0F ? stamina / max_stamina : 0.0F;
  }

  [[nodiscard]] auto can_start_running() const noexcept -> bool {
    return stamina >= k_min_stamina_to_start_run;
  }

  [[nodiscard]] auto has_stamina() const noexcept -> bool { return stamina > 0.0F; }

  void deplete(float delta_time) noexcept {
    stamina = std::max(0.0F, stamina - depletion_rate * delta_time);
  }

  void regenerate(float delta_time) noexcept {
    stamina = std::min(max_stamina, stamina + regen_rate * delta_time);
  }

  void initialize_from_stats(float new_max_stamina,
                             float new_regen_rate,
                             float new_depletion_rate) noexcept {
    max_stamina = new_max_stamina;
    stamina = new_max_stamina;
    regen_rate = new_regen_rate;
    depletion_rate = new_depletion_rate;
  }
};

class TerrainContextComponent : public Component {
public:
  TerrainContextComponent() = default;

  bool is_on_bridge{false};
  bool is_at_hill_entrance{false};
  float audio_cooldown{0.0F};

  static constexpr float k_audio_cooldown_time = 5.0F;
};

class ElephantComponent : public Component {
public:
  enum class ChargeState {
    Idle,
    Charging,
    Trampling,
    Recovering
  };

  ElephantComponent() = default;

  ChargeState charge_state{ChargeState::Idle};
  float charge_speed_multiplier{1.8F};
  float charge_duration{0.0F};
  float charge_cooldown{0.0F};
  float trample_radius{2.5F};
  int trample_damage{40};
  float trample_damage_accumulator{0.0F};

  float foot_lateral{0.153F};
  float foot_forward{0.280F};
};

class ElephantPanicComponent : public Component {
public:
  ElephantPanicComponent() = default;

  float duration{0.0F};
};

class ElephantStompImpactComponent : public Component {
public:
  struct ImpactRecord {
    float x;
    float z;
    float time;
  };

  ElephantStompImpactComponent() = default;

  std::vector<ImpactRecord> impacts;
};

class HomeComponent : public Component {
public:
  HomeComponent() = default;

  int population_contribution{50};
  Engine::Core::EntityID nearest_barracks_id{0};
  float update_cooldown{0.0F};
  float family_generation_cooldown{0.0F};
  float family_generation_interval{12.0F};
  int family_manpower_value{8};
};

class CivilianDeliveryComponent : public Component {
public:
  CivilianDeliveryComponent() = default;

  EntityID target_barracks_id{0};
};

class MovementIntentComponent : public Component {
public:
  MovementIntentComponent() = default;

  float desired_vx{0.0F};
  float desired_vz{0.0F};
  float desired_facing{0.0F};
  bool has_facing_request{false};

  float knockback_dx{0.0F};
  float knockback_dz{0.0F};

  std::uint8_t priority{0};
};

class EngagementSlotComponent : public Component {
public:
  EngagementSlotComponent() = default;

  EntityID target_id{0};

  std::uint8_t slot_index{0};
  std::uint8_t max_slots{8};

  float anchor_offset_x{0.0F};
  float anchor_offset_z{0.0F};

  bool valid{true};

  float lease_remaining{2.0F};
};

class TargetCommitmentComponent : public Component {
public:
  TargetCommitmentComponent() = default;

  EntityID committed_target_id{0};
  float cooldown_remaining{0.0F};

  bool in_committed_phase{false};

  static constexpr float k_switch_cooldown = 0.8F;
};

class CohortMembershipComponent : public Component {
public:
  CohortMembershipComponent() = default;

  std::uint32_t cohort_id{0};
  bool cohort_activated{false};
};

class ElephantKnockbackCooldownComponent : public Component {
public:
  ElephantKnockbackCooldownComponent() = default;

  struct VictimCooldown {
    EntityID victim_id{0};
    float remaining{0.0F};
  };

  std::vector<VictimCooldown> cooldowns;
  static constexpr float k_knockback_cooldown = 1.0F;

  void tick(float dt) {
    for (auto it = cooldowns.begin(); it != cooldowns.end();) {
      it->remaining -= dt;
      if (it->remaining <= 0.0F) {
        it = cooldowns.erase(it);
      } else {
        ++it;
      }
    }
  }

  [[nodiscard]] auto is_on_cooldown(EntityID victim) const -> bool {
    for (const auto& cd : cooldowns) {
      if (cd.victim_id == victim) {
        return true;
      }
    }
    return false;
  }

  void add_cooldown(EntityID victim) {
    cooldowns.push_back({victim, k_knockback_cooldown});
  }
};

} // namespace Engine::Core
