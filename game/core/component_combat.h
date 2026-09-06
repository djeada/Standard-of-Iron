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
#include "component_core.h"

namespace Engine::Core {

class AttackComponent {
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

  float min_range{0.0F};

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

  float melee_lock_separation_time{0.0F};

  float melee_footwork_offset{0.0F};

  static constexpr float k_melee_contact_range_grace = 0.75F;
  static constexpr float k_melee_lock_separation_release = 1.5F;

  [[nodiscard]] auto is_in_melee_range(float distance,
                                       float height_diff) const -> bool {
    return distance <= melee_range && height_diff <= max_height_difference;
  }

  [[nodiscard]] auto is_in_ranged_range(float distance) const -> bool {
    return distance <= range && distance > melee_range && distance >= min_range;
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

class AttackTargetComponent {
public:
  AttackTargetComponent() = default;

  EntityID target_id{0};
  bool should_chase{false};
  bool is_player_command{false};
};

class ThreatAlertComponent {
public:
  ThreatAlertComponent() = default;

  enum class Kind : std::uint8_t {
    UnderAttack,
    EnemySighted,
  };

  EntityID aggressor_id{0};
  Kind kind{Kind::UnderAttack};
  float cooldown{0.0F};
};

class RpgCommanderTargetComponent {
public:
  RpgCommanderTargetComponent() = default;

  static constexpr std::uint16_t k_no_soldier_slot =
      std::numeric_limits<std::uint16_t>::max();

  EntityID explicit_lock_target_id{0};
  std::uint16_t explicit_lock_soldier_slot{k_no_soldier_slot};
  EntityID aim_candidate_id{0};
  std::uint16_t aim_candidate_soldier_slot{k_no_soldier_slot};
  bool aim_candidate_in_range{false};
  EntityID recent_hit_target_id{0};
  std::uint16_t recent_hit_soldier_slot{k_no_soldier_slot};
  float recent_hit_timer{0.0F};

  std::uint32_t hit_confirm_sequence{0};

  bool recent_hit_killed{false};
};

enum class FpvWeaponStance : std::uint8_t {
  Melee = 0,
  Bow = 1,
};

enum class BowDrawStage : std::uint8_t {
  None = 0,

  Drawing,

  FullDraw,

  Loosing,
};

class RpgCommanderAimComponent {
public:
  RpgCommanderAimComponent() = default;

  static constexpr float k_min_shot_power = 0.35F;

  static constexpr float k_steady_hold_seconds = 2.0F;

  static constexpr float k_max_hold_seconds = 6.0F;

  static constexpr float k_exhausted_shot_power = 0.65F;

  float view_yaw_degrees{0.0F};
  float view_pitch_degrees{0.0F};
  float eye_height{1.55F};
  float move_speed{0.0F};
  bool running{false};

  float camera_origin_x{0.0F};
  float camera_origin_y{0.0F};
  float camera_origin_z{0.0F};
  bool camera_origin_valid{false};

  float camera_forward_x{0.0F};
  float camera_forward_y{0.0F};
  float camera_forward_z{1.0F};
  bool camera_forward_valid{false};

  float camera_fov_degrees{68.0F};

  FpvWeaponStance stance{FpvWeaponStance::Melee};
  bool stance_resolved{false};

  bool draw_held{false};
  BowDrawStage draw_stage{BowDrawStage::None};
  float draw_progress{0.0F};
  float full_draw_hold{0.0F};
  float spread_degrees{0.0F};
  float shot_power{0.0F};
  bool relaxed_from_overhold{false};
  std::uint32_t shot_sequence{0};

  [[nodiscard]] auto is_drawing() const noexcept -> bool {
    return draw_stage == BowDrawStage::Drawing || draw_stage == BowDrawStage::FullDraw;
  }
};

enum class RpgCommanderActionPhase : std::uint8_t {
  None,
  Strike,
  Guard,
  Dodge,
  Jump,
  Ability
};

class RpgCommanderActionComponent {
public:
  RpgCommanderActionComponent() = default;

  RpgCommanderActionPhase phase{RpgCommanderActionPhase::None};
  std::uint8_t combat_action_id{0};
  std::uint8_t melee_attack_sequence{0};
  std::uint8_t exchange_outcome{0};
  EntityID active_target_id{0};
  std::uint16_t active_target_soldier_slot{
      RpgCommanderTargetComponent::k_no_soldier_slot};
  EntityID last_hit_target_id{0};
  std::uint16_t last_hit_soldier_slot{RpgCommanderTargetComponent::k_no_soldier_slot};
  static constexpr std::size_t k_max_action_hit_targets = 8;
  std::array<EntityID, k_max_action_hit_targets> hit_target_ids{};
  std::array<std::uint16_t, k_max_action_hit_targets> hit_target_soldier_slots{};
  std::uint8_t hit_target_count{0};
  int last_damage{0};
  int requested_damage{0};

  float last_contact_speed{0.0F};
  float action_elapsed_time{0.0F};
  float action_duration{0.0F};
  float previous_normalized_action_time{0.0F};
  float normalized_action_time{0.0F};
  std::uint8_t next_event_index{0};
  std::uint8_t last_event_type{0};
  bool last_event_valid{false};
  bool action_active{false};
  bool weapon_trace_active{false};
  bool action_running{false};
  bool action_completed{false};
};

enum class CombatIntentOutcome : std::uint8_t {
  Accepted = 0,
  Buffered,
  InsufficientStamina,
  Recovering,
  Committed,
  GuardBroken,
  Staggered,
  NoFighter,
  Expired
};

enum class CommanderCombatIntentType : std::uint8_t {
  Light = 0,
  Heavy,
  Jump,
  Dodge,
  Guard,
  Special,
  WeaponSwitch
};

struct CombatActionIntent {

  CommanderCombatIntentType type{CommanderCombatIntentType::Light};

  MeleeIntent swing{};
  bool has_swing{false};

  float pressed_at{0.0F};
  float held_duration{0.0F};
};

class CombatIntentQueueComponent {
public:
  CombatIntentQueueComponent() = default;

  static constexpr std::size_t k_capacity = 3U;

  static constexpr float k_intent_lifetime = 0.22F;

  std::array<CombatActionIntent, k_capacity> entries{};
  std::uint8_t count{0};

  float clock{0.0F};

  CombatIntentOutcome last_outcome{CombatIntentOutcome::Accepted};
  float last_outcome_age{0.0F};

  std::uint32_t accepted_intents{0};
  std::uint32_t buffered_intents{0};
  std::uint32_t refused_intents{0};
  std::uint32_t expired_intents{0};
  std::uint32_t overflow_intents{0};

  [[nodiscard]] auto empty() const noexcept -> bool { return count == 0U; }

  [[nodiscard]] auto front() noexcept -> CombatActionIntent* {
    return count == 0U ? nullptr : &entries[0];
  }

  void push(const CombatActionIntent& intent) noexcept {
    if (count == k_capacity) {

      for (std::size_t i = 1; i < k_capacity; ++i) {
        entries[i - 1] = entries[i];
      }
      --count;
      ++overflow_intents;
    }
    entries[count++] = intent;
  }

  void pop_front() noexcept {
    if (count == 0U) {
      return;
    }
    for (std::size_t i = 1; i < count; ++i) {
      entries[i - 1] = entries[i];
    }
    --count;
  }

  void clear() noexcept { count = 0U; }

  void record(CombatIntentOutcome outcome) noexcept {
    if (outcome != last_outcome || outcome == CombatIntentOutcome::Accepted) {
      switch (outcome) {
      case CombatIntentOutcome::Accepted:
        ++accepted_intents;
        break;
      case CombatIntentOutcome::Buffered:
      case CombatIntentOutcome::Recovering:
      case CombatIntentOutcome::Committed:
        ++buffered_intents;
        break;
      case CombatIntentOutcome::Expired:
        ++expired_intents;
        break;
      case CombatIntentOutcome::InsufficientStamina:
      case CombatIntentOutcome::GuardBroken:
      case CombatIntentOutcome::Staggered:
      case CombatIntentOutcome::NoFighter:
        ++refused_intents;
        break;
      }
    }
    last_outcome = outcome;
    last_outcome_age = 0.0F;
  }
};

enum class RpgContactOutcome : std::uint8_t {
  Damage = 0,
  Block = 1,
  PerfectGuard = 2,
  Dodge = 3,
};

class RpgContactPresentationComponent {
public:
  struct Entry {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float age{0.0F};
    float lifetime{0.24F};
    float intensity{1.0F};
    RpgContactOutcome outcome{RpgContactOutcome::Damage};
  };

  static constexpr std::size_t k_max_entries = 6U;
  std::vector<Entry> entries;
};

enum class CommanderSignatureForm : std::uint8_t {
  Thrust = 0,
  Cut,
  Shot,
  Sweep,
  Slam,
};

enum class CommanderStrikeCue : std::uint8_t {
  Impact = 0,
  Swing,
};

class CommanderSignaturePresentationComponent {
public:
  struct Entry {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    float dir_x{0.0F};
    float dir_z{1.0F};
    float age{0.0F};
    float lifetime{0.34F};
    float intensity{1.0F};
    float reach{1.8F};
    float span{0.55F};
    CommanderSignatureForm form{CommanderSignatureForm::Cut};
    CommanderStrikeCue cue{CommanderStrikeCue::Impact};
  };

  static constexpr std::size_t k_max_entries = 6U;
  static constexpr float k_swing_lifetime = 0.46F;
  std::vector<Entry> entries;
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
    case SpawnType::RomanFieldCommander:
    case SpawnType::CarthageBowCommander:
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
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageBowCommander:
  case SpawnType::CarthageSwordCommander:
    return CombatAttackFamily::Sword;
  case SpawnType::Spearman:
  case SpawnType::HorseSpearman:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::CarthageSpearCommander:
    return CombatAttackFamily::Spear;
  case SpawnType::GravePriest:
    return CombatAttackFamily::Sword;
  default:
    return CombatAttackFamily::None;
  }
}

using CombatAnimationState = Animation::CombatPhase;

enum class AttackDirection : std::uint8_t {
  LeftSlash = 0,
  RightSlash = 1,
  Overhead = 2,
  Thrust = 3,
  HeavyOverhead = 4
};

[[nodiscard]] inline auto
classify_attack_direction(const MeleeIntent& raw,
                          bool heavy = false) noexcept -> AttackDirection {
  MeleeIntent const intent = normalized_melee_intent(raw);

  if (intent.thrust_amount >= 0.55F) {
    return AttackDirection::Thrust;
  }

  bool const descending = intent.strike_dir_y < -0.10F;
  bool const vertical = std::abs(intent.strike_dir_y) > std::abs(intent.strike_dir_x);
  if (descending && vertical) {
    return (heavy || intent.charge >= 0.60F) ? AttackDirection::HeavyOverhead
                                             : AttackDirection::Overhead;
  }

  return intent.strike_dir_x < 0.0F ? AttackDirection::LeftSlash
                                    : AttackDirection::RightSlash;
}

[[nodiscard]] inline auto melee_intent_from_attack_direction(
    AttackDirection direction,
    float reach = k_melee_default_reach) noexcept -> MeleeIntent {
  switch (direction) {
  case AttackDirection::RightSlash:
    return melee_intent_from_strike_angle(
        Animation::k_melee_right_cut_angle, 0.0F, reach);
  case AttackDirection::Overhead:
    return melee_intent_from_strike_angle(
        Animation::k_melee_overhead_angle, 0.0F, reach);
  case AttackDirection::HeavyOverhead: {
    MeleeIntent intent =
        melee_intent_from_strike_angle(Animation::k_melee_overhead_angle, 0.0F, reach);
    intent.charge = 1.0F;
    intent.follow_through = 0.85F;
    complete_melee_intent(intent, reach);
    return intent;
  }
  case AttackDirection::Thrust:
    return melee_intent_from_strike_angle(Animation::k_melee_thrust_angle, 1.0F, reach);
  case AttackDirection::LeftSlash:
    break;
  }
  return melee_intent_from_strike_angle(Animation::k_melee_left_cut_angle, 0.0F, reach);
}

enum class TelegraphCue : std::uint8_t {
  None = 0,

  Warning,

  Flash,

  Impact,

  Settling
};

class CombatStateComponent {
public:
  CombatStateComponent() = default;

  CombatAnimationState animation_state{CombatAnimationState::Idle};
  CombatAttackFamily attack_family{CombatAttackFamily::None};

  MeleeIntent intent{};

  [[nodiscard]] auto attack_direction() const noexcept -> AttackDirection {
    return classify_attack_direction(intent, finisher_attack);
  }

  float state_time{0.0F};
  float state_duration{0.0F};
  float attack_offset{0.0F};
  std::uint8_t attack_variant{0};
  bool finisher_attack{false};
  bool is_hit_paused{false};
  float hit_pause_remaining{0.0F};
  bool damage_dealt_this_swing{false};

  bool tired_swing{false};
  bool input_buffered{false};

  float telegraph_intensity{0.0F};
  TelegraphCue telegraph_cue{TelegraphCue::None};

  static constexpr float k_combat_animation_hit_pause_duration = 0.10F;

  static constexpr float k_player_hit_pause_duration = 0.045F;
  static constexpr float k_advance_duration = 0.22F;
  static constexpr float k_wind_up_duration = 0.42F;
  static constexpr float k_strike_duration = 0.34F;
  static constexpr float k_impact_duration = 0.18F;
  static constexpr float k_recover_duration = 0.40F;
  static constexpr float k_reposition_duration = 0.28F;

  static constexpr std::uint8_t k_attack_variant_seed_slots = 8;

  static constexpr float k_stamina_cost_light_attack = 30.0F;
  static constexpr float k_stamina_cost_heavy_attack = 50.0F;
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

enum class HitReactionKind : std::uint8_t {
  Flinch = 0,
  Block = 1,
  Evade = 2,
  Stagger = 3,
  Recoil = 4,
};

[[nodiscard]] constexpr auto
hit_reaction_duration(HitReactionKind kind) noexcept -> float {
  switch (kind) {
  case HitReactionKind::Flinch:
    return 0.30F;
  case HitReactionKind::Block:
    return 0.34F;
  case HitReactionKind::Evade:
    return 0.36F;
  case HitReactionKind::Stagger:
    return 0.60F;
  case HitReactionKind::Recoil:
    return 0.26F;
  }
  return 0.30F;
}

class HitFeedbackComponent {
public:
  HitFeedbackComponent() = default;

  EntityID source_attacker_id{0};
  bool is_reacting{false};
  float reaction_time{0.0F};
  float reaction_duration{k_reaction_duration};
  float reaction_intensity{0.0F};
  float knockback_x{0.0F};
  float knockback_z{0.0F};
  float knockback_applied{0.0F};
  StaggerTier stagger_tier{StaggerTier::LightFlinch};
  HitReactionKind reaction_kind{HitReactionKind::Flinch};
  float hit_direction_x{0.0F};
  float hit_direction_z{0.0F};

  float recent_damage_remaining{0.0F};

  static constexpr float k_recent_damage_window = 3.0F;
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

class PatrolComponent {
public:
  PatrolComponent() = default;

  std::vector<std::pair<float, float>> waypoints;
  size_t current_waypoint{0};
  bool patrolling{false};
};

} // namespace Engine::Core
