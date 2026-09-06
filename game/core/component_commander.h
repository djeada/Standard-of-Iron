#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../animation/combat_manifest.h"
#include "../systems/nation_id.h"
#include "../systems/projectile_kind.h"
#include "../systems/resource_types.h"
#include "../systems/unit_activity.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "../wildlife/wildlife_species.h"
#include "entity.h"
#include "melee_intent.h"
#include "movement_facts.h"

namespace Game::Systems {
class MovementSystem;
class RouteFollowSystem;
} // namespace Game::Systems
#include "component_structures.h"

namespace Engine::Core {

class CommanderComponent {
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
    rally_requested = true;
  }

  [[nodiscard]] auto is_flag_rally_planting() const noexcept -> bool {
    return flag_rally_in_progress && flag_rally_at_position;
  }

  [[nodiscard]] auto can_activate_aura_ability() const noexcept -> bool {
    return aura_active && !wounded && !aura_ability_active &&
           aura_ability_cooldown_remaining <= 0.0F;
  }

  auto request_aura_ability() noexcept -> bool {
    if (!can_activate_aura_ability()) {
      return false;
    }
    aura_ability_requested = true;
    return true;
  }

  std::string commander_id;
  std::string display_name;
  std::string strategic_identity;
  std::string passive_aura;
  std::string bonus_type;
  std::string bonus_summary;
  std::string rally_ability;
  std::string death_consequence;
  int bodyguard_count{0};
  float aura_radius{12.0F};
  float aura_morale_bonus{5.0F};
  float aura_bonus_value{0.0F};
  float rally_range{10.0F};
  float rally_cooldown{45.0F};
  float rally_morale_restore{25.0F};
  float rally_cooldown_remaining{0.0F};
  float rally_feedback_time{0.0F};
  bool rally_requested{false};
  bool rally_requires_manual_trigger{true};
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

  std::uint8_t signature_move{0U};
  std::string signature_name;
  float signature_cooldown{9.0F};
  float signature_cooldown_remaining{0.0F};
  float signature_damage_multiplier{1.0F};
  float signature_bonus_reach{0.0F};
  float signature_stagger_seconds{0.0F};
  int signature_max_targets{1};

  bool signature_strike_active{false};

  bool wounded{false};
  bool fpv_controlled{false};
  bool advanced_combat_enabled{true};
  int combo_step{0};
  std::uint8_t combo_action_id{0U};
  float combo_window_remaining{0.0F};
  bool power_strike_active{false};
  float shield_bash_cooldown_remaining{0.0F};
  float vanguard_rush_cooldown_remaining{0.0F};
  float second_wind_cooldown_remaining{0.0F};
  bool just_struck_enemy{false};
  std::uint8_t last_strike_combo_step{0U};
  bool jump_active{false};
  float jump_phase{0.0F};
  float jump_height_offset{0.0F};
  float airborne_velocity{0.0F};
  bool dodge_active{false};
  float dodge_phase{0.0F};
  bool dive_attack_active{false};
  float fpv_motion_vx{0.0F};
  float fpv_motion_vz{0.0F};
  bool fpv_motion_requested{false};
  static constexpr float k_direct_control_gait_floor_speed = 0.60F;
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

class CommanderBodyControlComponent {
public:
  CommanderBodyControlComponent() = default;

  static constexpr float k_max_steer_rate = 7.5F;

  static constexpr float k_sweep_degrees = 45.0F;

  MeleeIntent steered_intent{};

  float steer_x{0.0F};
  float steer_y{0.0F};
  float steer_rate{0.0F};

  bool swing_in_flight{false};

  float rest_dir_x{0.80F};
  float rest_dir_y{0.60F};
  bool rest_valid{false};

  float last_strike_dir_x{0.0F};
  float last_strike_dir_y{0.0F};
  float chain_window_remaining{0.0F};

  static constexpr float k_chain_window_seconds = 0.55F;

  static constexpr float k_chain_new_line_dot = 0.82F;
};

class CommanderGuardComponent {
public:
  CommanderGuardComponent() = default;

  float guard_dir_x{0.0F};
  float guard_dir_y{1.0F};

  float guard_turn_rate{0.0F};

  bool active{false};
  float frontal_arc_dot{0.15F};
  float damage_multiplier{0.0F};
  float perfect_guard_remaining{0.0F};
  float guard_break_remaining{0.0F};
  bool rearm_requires_release{false};
};

enum class SpearBraceSource : std::uint8_t {
  Explicit = 0,
  HoldMode = 1,
  GuardMode = 2,
  CommanderGuard = 3
};

class SpearBraceComponent {
public:
  SpearBraceComponent() = default;

  bool requested{false};
  bool active{false};
  float pose_progress{0.0F};
  float enter_duration{0.28F};
  float exit_duration{0.18F};
  float min_interrupt_speed{3.5F};
  SpearBraceSource source{SpearBraceSource::Explicit};
};

enum class MountedChargeState : std::uint8_t {
  Ready = 0,
  Charging = 1,
  ImpactActive = 2,
  Cooldown = 3
};

enum class MountedChargeIntentSource : std::uint8_t {
  None = 0,
  Player = 1,
  AI = 2,
  ContactAuto = 3
};

enum class MountedChargeCancelReason : std::uint8_t {
  None = 0,
  SpeedLost = 1,
  Interrupted = 2,
  InvalidUnit = 3,
  Explicit = 4
};

class MountedChargeComponent {
public:
  MountedChargeComponent() = default;

  MountedChargeState state{MountedChargeState::Ready};
  MountedChargeIntentSource intent_source{MountedChargeIntentSource::None};
  MountedChargeCancelReason last_cancel_reason{MountedChargeCancelReason::None};
  bool intent_requested{false};
  bool auto_contact_enabled{true};
  float min_start_speed{4.0F};
  float cancel_speed{2.2F};
  float speed_loss_grace{0.15F};
  float below_cancel_speed_time{0.0F};
  float last_observed_speed{0.0F};
  float cooldown_duration{0.65F};
  float cooldown_remaining{0.0F};
  float impact_speed{0.0F};
  EntityID active_target_id{0};
  EntityID last_impact_target_id{0};
};

class RpgHealthComponent {
public:
  RpgHealthComponent() = default;

  float incoming_damage_scale{1.0F};

  float armor{0.0F};
  float crit_chance{0.05F};
  float crit_multiplier{1.8F};
  bool active{false};

  float dodge_grace_remaining{0.0F};
  float dodge_dir_x{0.0F};
  float dodge_dir_z{0.0F};

  std::uint32_t blocked_contacts{0};
  std::uint32_t perfect_guard_contacts{0};
  std::uint32_t dodged_contacts{0};
  std::uint32_t damaging_contacts{0};
  std::uint32_t guard_broken_contacts{0};
  RpgContactOutcome last_contact_outcome{RpgContactOutcome::Damage};
};

class StaggerComponent {
public:
  explicit StaggerComponent(float duration = 0.5F)
      : remaining(duration) {}
  float remaining;
  StaggerTier tier{StaggerTier::LightFlinch};
};

class PoiseComponent {
public:
  explicit PoiseComponent(float capacity = 100.0F)
      : current(std::max(1.0F, capacity))
      , maximum(std::max(1.0F, capacity)) {}

  float current{100.0F};
  float maximum{100.0F};
  float regeneration_per_second{28.0F};
  float regeneration_delay{0.0F};

  static constexpr float k_regeneration_delay_seconds = 1.25F;
};

class CombatLaunchComponent {
public:
  CombatLaunchComponent() = default;
  CombatLaunchComponent(float vertical_velocity,
                        float horizontal_velocity_x,
                        float horizontal_velocity_z,
                        float landing_height)
      : velocity_y(vertical_velocity)
      , velocity_x(horizontal_velocity_x)
      , velocity_z(horizontal_velocity_z)
      , ground_y(landing_height) {}

  float velocity_y{0.0F};
  float velocity_x{0.0F};
  float velocity_z{0.0F};
  float ground_y{0.0F};

  static constexpr float k_gravity = 18.0F;
  static constexpr float k_horizontal_drag = 5.0F;
};

enum class FightContext : std::uint8_t {
  None,
  Duel,
  Skirmish
};

class RpgEngagementComponent {
public:
  struct Slot {
    EntityID entity_id{0};
    float distance{0.0F};
    float signed_angle_degrees{0.0F};

    bool obstructed{false};

    bool pressing{false};
  };

  struct PressTenure {
    EntityID entity_id{0};
    float started_at{0.0F};
  };

  std::vector<Slot> engagement_slots;
  float ring_radius{5.0F};

  float pressure_clock{0.0F};
  FightContext fight_context{FightContext::None};

  std::vector<PressTenure> press_tenure;

  [[nodiscard]] auto pressing_count() const noexcept -> int {
    int pressing = 0;
    for (auto const& slot : engagement_slots) {
      pressing += slot.pressing ? 1 : 0;
    }
    return pressing;
  }

  [[nodiscard]] auto is_pressing(EntityID entity_id) const noexcept -> bool {
    for (auto const& slot : engagement_slots) {
      if (slot.entity_id == entity_id) {
        return slot.pressing;
      }
    }
    return false;
  }
};

} // namespace Engine::Core
