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
#include "component_commander.h"

namespace Engine::Core {

class AIControlledComponent {
public:
  AIControlledComponent() = default;
};

class CaptureComponent {
public:
  CaptureComponent()
      : required_time(Defaults::k_capture_required_time) {}

  int capturing_player_id{-1};
  float capture_progress{0.0F};
  float required_time;
  bool is_being_captured{false};

  bool capture_blocked{false};
};

enum class BuilderTaskFault : std::uint8_t {
  None = 0,

  Interrupted = 1,

  TargetLost = 2,

  Unreachable = 3,
};

class BuilderProductionComponent {
public:
  BuilderProductionComponent() = default;

  void report_fault(BuilderTaskFault reported_fault, float display_seconds = 6.0F) {
    fault = reported_fault;
    fault_display_remaining = display_seconds;
  }

  void clear_fault() {
    fault = BuilderTaskFault::None;
    fault_display_remaining = 0.0F;
  }

  [[nodiscard]] auto has_active_fault() const -> bool {
    return fault != BuilderTaskFault::None && fault_display_remaining > 0.0F;
  }

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
  EntityID construction_site_entity_id{0};
  std::vector<EntityID> queued_construction_site_ids{};

  bool bypass_movement_active{false};
  float bypass_target_x{0.0F};
  float bypass_target_z{0.0F};

  float site_approach_seconds{0.0F};

  EntityID structure_task_entity_id{0};

  bool has_gather_order{false};
  std::string gather_product_type{};
  float gather_anchor_x{0.0F};
  float gather_anchor_z{0.0F};

  bool auto_gather{false};

  std::string auto_gather_priority{};

  BuilderTaskFault fault{BuilderTaskFault::None};
  float fault_display_remaining{0.0F};

  void clear_gather_order() {
    has_gather_order = false;
    gather_product_type.clear();
    gather_anchor_x = 0.0F;
    gather_anchor_z = 0.0F;
  }

  void clear_auto_gather() {
    auto_gather = false;
    auto_gather_priority.clear();
  }
};

class WallSegmentComponent {
public:
  WallSegmentComponent() = default;

  int grid_x{0};
  int grid_z{0};
  std::uint8_t connection_mask{0};

  bool freeform{false};

  [[nodiscard]] static auto is_freeform_rotation(float rotation_y) -> bool {
    float angle = std::fmod(rotation_y, 90.0F);
    if (angle < 0.0F) {
      angle += 90.0F;
    }
    constexpr float k_quarter_turn_tolerance = 0.5F;
    return angle > k_quarter_turn_tolerance && angle < 90.0F - k_quarter_turn_tolerance;
  }
};

class WallConstructionSiteComponent {
public:
  WallConstructionSiteComponent() = default;

  int owner_id{0};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  float build_time{0.0F};
  float progress{0.0F};

  Game::Units::SpawnType product_type{Game::Units::SpawnType::WallSegment};
};

class DismantleSiteComponent {
public:
  DismantleSiteComponent() = default;

  float duration{1.0F};
  float progress{0.0F};
  int active_workers{0};
};

class GateComponent {
public:
  enum class State : std::uint8_t {
    Closed = 0,
    Opening,
    Open,
    Closing
  };

  enum class ManualMode : std::uint8_t {
    Automatic = 0,
    ForcedOpen,
    ForcedClosed
  };

  static constexpr float k_structure_half_span = 4.5F;
  static constexpr float k_passage_half_width = 2.85F;
  static constexpr float k_cross_half_extent = 1.5F;

  static constexpr float k_passable_open_amount = 0.94F;
  static constexpr float k_blocking_open_amount = 0.25F;

  GateComponent() = default;

  State state{State::Closed};
  ManualMode manual_mode{ManualMode::Automatic};

  float open_amount{0.0F};
  float open_speed{2.4F};
  float close_speed{2.0F};
  float trigger_radius{5.0F};
  float hold_open_seconds{2.5F};
  float hold_timer{0.0F};

  [[nodiscard]] auto is_passable() const -> bool {
    return open_amount >= k_passable_open_amount;
  }

  [[nodiscard]] auto blocks_movement() const -> bool {
    return open_amount < k_passable_open_amount;
  }

  [[nodiscard]] static auto spans_x_axis(float rotation_y) -> bool {
    float angle = std::fmod(rotation_y, 180.0F);
    if (angle < 0.0F) {
      angle += 180.0F;
    }
    return angle < 45.0F || angle >= 135.0F;
  }
};

class ConstructionPreviewComponent {
public:
  ConstructionPreviewComponent() = default;

  int owner_id{0};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  int grid_x{0};
  int grid_z{0};
  bool valid{false};
};

class PendingRemovalComponent {
public:
  PendingRemovalComponent() = default;
};

class BloodStainComponent {
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

class FirePatchComponent {
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

class StructureFireComponent {
public:
  StructureFireComponent() = default;

  float ignition_progress{0.0F};
  float ignition_threshold{1.0F};
  float duration{0.0F};
  float remaining_duration{0.0F};
  float ignition_elapsed{0.0F};
  float tick_interval{0.75F};
  float tick_accumulator{0.0F};
  int damage_per_tick{0};
  EntityID attacker_id{0};

  [[nodiscard]] auto is_burning() const -> bool { return remaining_duration > 0.0F; }
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

class DeathAnimationComponent {
public:
  DeathAnimationComponent() = default;

  DeathSequenceProfile profile{DeathSequenceProfile::Infantry};
  DeathSequenceState state{DeathSequenceState::Dying};
  float state_time{0.0F};
  float state_duration{1.0F};
  float dead_hold_duration{0.8F};
  std::uint8_t sequence_variant{0};
};

class SoldierCasualtyAnimationComponent {
public:
  struct Entry {
    std::uint16_t slot_index{0};
    bool has_local_anchor{false};
    float local_x{0.0F};
    float local_z{0.0F};
    float local_yaw{0.0F};
    DeathSequenceProfile profile{DeathSequenceProfile::Infantry};
    DeathSequenceState state{DeathSequenceState::Dying};
    float state_time{0.0F};
    float state_duration{1.0F};
    float dead_hold_duration{0.8F};
    std::uint8_t sequence_variant{0};
    bool launched{false};
    float launch_velocity_x{0.0F};
    float launch_velocity_y{0.0F};
    float launch_velocity_z{0.0F};

    float launch_pitch_speed{0.0F};
    float launch_roll_speed{0.0F};
  };

  std::vector<Entry> entries;
};

class AssaultWaveComponent {
public:
  AssaultWaveComponent() = default;

  bool active{true};
  int wave_phase{0};

  bool has_march_target{false};
  float march_target_x{0.0F};
  float march_target_z{0.0F};
};

class HoldModeComponent {
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

class GuardModeComponent {
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

class HealerComponent {
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

class SpecialAttackComponent {
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

class CatapultLoadingComponent {
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

  Game::Systems::ProjectileKind loaded_projectile_kind{
      Game::Systems::ProjectileKind::Stone};

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

class FormationModeComponent {
public:
  FormationModeComponent() = default;

  bool active{false};
  float formation_center_x{0.0F};
  float formation_center_z{0.0F};
  std::uint64_t formation_id{0};
  int stable_slot_id{-1};
  int stable_rank{-1};
  int stable_file{-1};
  float stable_slot_x{0.0F};
  float stable_slot_z{0.0F};
};

class ArmyFormationMembershipComponent {
public:
  ArmyFormationMembershipComponent() = default;

  std::uint64_t group_id{0};
  int slot_id{-1};

  [[nodiscard]] auto is_valid() const noexcept -> bool { return group_id != 0U; }
};

class UnitLayoutStateComponent {
public:
  UnitLayoutStateComponent() = default;

  std::uint8_t state{0U};
  std::uint8_t phase{1U};
  float transition_progress{1.0F};
  float transition_seconds{0.0F};
  std::uint16_t layout_id{0xFFFFU};
  std::uint16_t requested_layout_id{0xFFFFU};

  std::uint16_t previous_layout_id{0xFFFFU};

  [[nodiscard]] auto is_formed() const noexcept -> bool { return phase == 1U; }
};

struct UnitTraversalSlotState {
  std::uint16_t slot_index{0U};
  std::uint16_t row{0U};
  std::uint16_t col{0U};
  float start_local_x{0.0F};
  float start_local_z{0.0F};
  float previous_local_x{0.0F};
  float previous_local_z{0.0F};
  float current_local_x{0.0F};
  float current_local_z{0.0F};
  float target_local_x{0.0F};
  float target_local_z{0.0F};
  float velocity_x{0.0F};
  float velocity_z{0.0F};
  bool alive{false};
  bool blocked{false};

  auto operator==(const UnitTraversalSlotState&) const -> bool = default;
};

class UnitTraversalLayoutStateComponent {
public:
  std::uint64_t route_id{0U};
  std::uint32_t portal_id{0U};
  std::uint16_t normal_layout_id{0xFFFFU};

  TraversalLayoutMode mode{TraversalLayoutMode::Normal};
  TraversalLayoutMode target_mode{TraversalLayoutMode::Normal};
  std::uint32_t normal_files{1U};
  std::uint32_t current_files{1U};
  std::uint32_t target_files{1U};
  std::vector<std::uint16_t> stable_slot_mapping;
  std::vector<UnitTraversalSlotState> slot_states;
  std::uint32_t slot_states_revision{0U};

  float entry_progress{0.0F};
  float exit_progress{1.0F};
  float transition_progress{1.0F};
  float transition_curve{1.0F};
  float transition_seconds{0.0F};
  float transition_total_distance{0.0F};
  float transition_remaining_distance{0.0F};
  float mode_dwell_seconds{0.0F};
  float tail_clear_seconds{0.0F};
  std::uint32_t blocked_slot_count{0U};
  bool root_motion_blocked{false};

  float lateral_scale{1.0F};
  float target_lateral_scale{1.0F};
  float available_half_width{0.0F};
  float desired_half_width{0.0F};
  float soldier_body_radius{0.0F};
  float file_spacing{0.0F};
  float rank_spacing{0.0F};
  float minimum_lateral_scale{1.0F};

  bool active{false};

  [[nodiscard]] auto
  slot_for(std::uint16_t slot_index) const noexcept -> const UnitTraversalSlotState* {
    if (slot_index < slot_states.size() &&
        slot_states[slot_index].slot_index == slot_index) {
      return &slot_states[slot_index];
    }
    auto const slot = std::find_if(
        slot_states.begin(), slot_states.end(), [slot_index](auto const& candidate) {
          return candidate.slot_index == slot_index;
        });
    return slot != slot_states.end() ? &*slot : nullptr;
  }
};

class StaminaComponent {
public:
  static constexpr float k_run_speed_multiplier = 2.0F;
  static constexpr float k_min_stamina_to_start_run = 10.0F;
  static constexpr float k_default_max_stamina = 200.0F;
  static constexpr float k_default_regen_rate = 10.0F;
  static constexpr float k_default_depletion_rate = 20.0F;

  StaminaComponent() noexcept = default;

  static constexpr float k_regen_delay_seconds = 0.75F;

  float stamina{k_default_max_stamina};
  float max_stamina{k_default_max_stamina};
  float regen_rate{k_default_regen_rate};
  float depletion_rate{k_default_depletion_rate};
  bool is_running{false};
  bool run_requested{false};

  float regen_delay_remaining{0.0F};

  void spend(float cost) noexcept {
    if (cost <= 0.0F) {
      return;
    }
    stamina = stamina - cost > 0.0F ? stamina - cost : 0.0F;
    regen_delay_remaining = k_regen_delay_seconds;
  }

  [[nodiscard]] auto get_stamina_ratio() const noexcept -> float {
    return max_stamina > 0.0F ? stamina / max_stamina : 0.0F;
  }

  [[nodiscard]] auto can_start_running() const noexcept -> bool {
    return stamina >= k_min_stamina_to_start_run;
  }

  [[nodiscard]] auto has_stamina() const noexcept -> bool { return stamina > 0.0F; }

  void initialize_from_stats(float new_max_stamina,
                             float new_regen_rate,
                             float new_depletion_rate) noexcept {
    max_stamina = new_max_stamina;
    stamina = new_max_stamina;
    regen_rate = new_regen_rate;
    depletion_rate = new_depletion_rate;
  }
};

class TerrainContextComponent {
public:
  TerrainContextComponent() = default;

  bool is_on_bridge{false};
  bool is_at_hill_entrance{false};
  float audio_cooldown{0.0F};

  static constexpr float k_audio_cooldown_time = 5.0F;
};

class ElephantComponent {
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

class ElephantPanicComponent {
public:
  ElephantPanicComponent() = default;

  float duration{0.0F};
};

class ElephantStompImpactComponent {
public:
  struct ImpactRecord {
    float x;
    float z;
    float time;
  };

  ElephantStompImpactComponent() = default;

  std::vector<ImpactRecord> impacts;
};

class StructureDamagePresentationComponent {
public:
  struct ImpactRecord {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float normal_x{0.0F};
    float normal_z{-1.0F};
    float age{0.0F};
    float lifetime{1.0F};
    float radius{0.45F};
    float intensity{1.0F};
    std::uint8_t style{0};
  };

  StructureDamagePresentationComponent() = default;

  std::vector<ImpactRecord> impacts;
};

} // namespace Engine::Core
