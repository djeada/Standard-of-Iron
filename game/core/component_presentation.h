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
#include "component_economy.h"

namespace Engine::Core {

class MovementIntentComponent {
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

class EngagementSlotComponent {
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

struct FormationEngagementPair {
  std::uint16_t attacker_slot{0};
  std::uint16_t target_slot{0};
  float root_distance{0.0F};
  float surface_gap{0.0F};

  auto operator==(const FormationEngagementPair&) const -> bool = default;
};

struct FormationContactFront {
  EntityID opponent_id{0};
  float surface_gap{0.0F};
  bool in_contact{false};
  bool outgoing{false};
  std::vector<FormationEngagementPair> engagement_pairs;

  auto operator==(const FormationContactFront&) const -> bool = default;
};

class FormationContactComponent {
public:
  FormationContactComponent() = default;

  EntityID target_id{0};
  float surface_gap{0.0F};
  bool in_contact{false};
  std::uint32_t revision{0};
  std::vector<std::uint16_t> engaged_soldier_indices;

  std::vector<FormationEngagementPair> engagement_pairs;
  std::vector<FormationContactFront> fronts;
};

enum class FormationSoldierAction : std::uint8_t {
  FollowUnit,
  MeleeReady,
  MeleeEngaged,
  MeleeGuard,
  MeleeReposition,
  MeleeFollowThrough,
};

enum class FormationSoldierCombatRole : std::uint8_t {
  None,
  LeadStrike,
  SupportStrike,
  Guard,
  StepIn,
  StepOut,
  Ready,
};

struct FormationSoldierPresentation {
  std::uint16_t slot_index{0};
  std::uint16_t row{0};
  std::uint16_t col{0};
  float local_x{0.0F};
  float local_z{0.0F};
  float previous_local_x{0.0F};
  float previous_local_z{0.0F};
  float relocation_velocity_x{0.0F};
  float relocation_velocity_z{0.0F};
  bool relocation_blocked{false};
  float local_yaw{0.0F};
  float contact_offset_x{0.0F};
  float contact_offset_z{0.0F};
  float target_held_seconds{0.0F};
  bool alive{false};
  FormationSoldierAction action{FormationSoldierAction::FollowUnit};
  FormationSoldierCombatRole combat_role{FormationSoldierCombatRole::None};
  EntityID opponent_id{0};
  std::uint16_t target_slot{0};
  float engagement_surface_gap{0.0F};
  float combat_phase_bias{0.0F};
  float combat_speed_scale{1.0F};
  bool damage_carrier{false};
  float unassigned_seconds{0.0F};

  auto operator==(const FormationSoldierPresentation&) const -> bool = default;
};

class FormationRosterPresentationComponent {
public:
  FormationRosterPresentationComponent() = default;

  std::uint16_t total_count{0};
  std::uint16_t live_count{0};
  std::uint32_t revision{0};
  std::vector<std::uint8_t> alive;
};

class FormationHitPresentationComponent {
public:
  FormationHitPresentationComponent() = default;

  EntityID attacker_id{0};
  std::uint16_t soldier_slot{0};
  float remaining{0.0F};
  float duration{0.0F};
  float intensity{0.0F};
  HitReactionKind reaction_kind{HitReactionKind::Flinch};
  float hit_direction_x{0.0F};
  float hit_direction_z{0.0F};
  std::uint32_t revision{0};
};

class FormationPresentationComponent {
public:
  FormationPresentationComponent() = default;

  std::uint32_t formation_seed{0};
  std::uint16_t rows{1};
  std::uint16_t cols{1};
  float spacing{0.75F};
  EntityID target_id{0};
  bool target_alive{false};
  bool melee_ordered{false};
  bool allow_full_body_hit_reaction{true};
  float combat_motion_time{0.0F};
  std::uint32_t revision{0};
  std::vector<FormationSoldierPresentation> soldiers;
};

enum class CreatureCastPresentation : std::uint8_t {
  None,
  Fireball,
};

class CommanderPresentationSampleComponent {
public:
  CommanderPresentationSampleComponent() = default;

  using Vec3 = TransformComponent::Vec3;

  bool valid{false};
  bool snap{true};
  std::uint32_t tick_sequence{0};
  float tick_seconds{0.0F};

  Vec3 previous_position{0.0F, 0.0F, 0.0F};
  float previous_yaw{0.0F};
  Vec3 position{0.0F, 0.0F, 0.0F};
  float yaw{0.0F};
};

struct PresentationPose {
  TransformComponent::Vec3 position{0.0F, 0.0F, 0.0F};
  float yaw{0.0F};
  float alpha{1.0F};
  bool extrapolated{false};
};

inline constexpr float k_presentation_max_extrapolation = 0.5F;

inline constexpr float k_presentation_teleport_threshold = 2.0F;

[[nodiscard]] inline auto
resolve_presentation_pose(const CommanderPresentationSampleComponent& sample,
                          float age_seconds) -> PresentationPose {
  PresentationPose pose;
  pose.position = sample.position;
  pose.yaw = sample.yaw;
  if (!sample.valid || sample.snap || sample.tick_seconds <= 0.0F) {
    return pose;
  }

  float const alpha = std::clamp(
      age_seconds / sample.tick_seconds, 0.0F, 1.0F + k_presentation_max_extrapolation);
  pose.alpha = alpha;
  pose.extrapolated = alpha > 1.0F;
  pose.position = {sample.previous_position.x +
                       ((sample.position.x - sample.previous_position.x) * alpha),
                   sample.previous_position.y +
                       ((sample.position.y - sample.previous_position.y) * alpha),
                   sample.previous_position.z +
                       ((sample.position.z - sample.previous_position.z) * alpha)};

  float delta = sample.yaw - sample.previous_yaw;
  while (delta > 180.0F) {
    delta -= 360.0F;
  }
  while (delta < -180.0F) {
    delta += 360.0F;
  }
  pose.yaw = sample.previous_yaw + (delta * alpha);
  return pose;
}

class CreaturePresentationComponent {
public:
  CreaturePresentationComponent() = default;

  bool snapshot_valid{false};
  std::uint32_t revision{0};

  EntityID target_id{0};
  bool target_alive{false};
  bool combat_active{false};
  bool attack_from_combat_state{false};
  bool attack_from_melee_lock{false};
  bool is_attacking{false};
  bool is_melee{false};
  bool is_in_melee_lock{false};
  CombatAnimationState combat_phase{CombatAnimationState::Idle};
  float combat_phase_progress{0.0F};

  MeleeIntent melee_intent{};
  bool melee_intent_valid{false};
  float melee_rest_x{0.80F};
  float melee_rest_y{0.60F};
  bool melee_rest_valid{false};

  CombatAttackFamily attack_family{CombatAttackFamily::None};
  std::uint8_t attack_variant{0};
  bool finisher_attack{false};
  float attack_offset{0.0F};
  bool has_attack_offset{false};

  bool is_casting{false};
  CreatureCastPresentation cast{CreatureCastPresentation::None};
  bool is_hit_reacting{false};
  float hit_reaction_intensity{0.0F};
  float hit_reaction_progress{0.0F};
  HitReactionKind hit_reaction_kind{HitReactionKind::Flinch};
  float hit_recoil_x{0.0F};
  float hit_recoil_z{0.0F};
  bool allow_full_body_hit_reaction{true};

  bool is_healing{false};
  float healing_target_dx{0.0F};
  float healing_target_dz{0.0F};
  bool is_constructing{false};
  float construction_progress{0.0F};
  std::uint8_t construction_job{0};
  bool is_dying{false};
  bool is_dead{false};
  float death_progress{0.0F};
  std::uint8_t death_variant{0};

  bool guard_requested{false};
  bool formation_guard_active{false};
  bool defensive_layout_locked{false};
  Game::Systems::UnitActivity activity{};
  bool hold_requested{false};
  bool hold_exit_requested{false};
  float hold_entry_progress{0.0F};
  float hold_exit_progress{0.0F};
  float hold_enter_duration{Defaults::k_hold_kneel_duration};
  float hold_exit_duration{Defaults::k_hold_stand_up_duration};

  bool fpv_controlled{false};
  bool has_commander{false};
  bool jump_active{false};
  float jump_phase{0.0F};
  float jump_height_offset{0.0F};
  bool dodge_active{false};
  float dodge_phase{0.0F};
  bool flag_rally_planting{false};
  float flag_rally_animation_timer{0.0F};
  float flag_rally_cost{0.0F};
  std::uint8_t authored_action_id{0};
  bool authored_action_running{false};
  bool authored_action_completed{false};
  float authored_action_phase{0.0F};
  std::uint8_t authored_action_exchange_outcome{0};

  bool showcase_active{false};
  std::uint8_t showcase_move{0};
  float showcase_phase{0.0F};
};

class ShowcaseRoutineComponent {
public:
  ShowcaseRoutineComponent() = default;

  struct Step {
    std::uint8_t move{0};
    float duration{0.0F};
    float hold_after{0.0F};
  };

  std::vector<Step> steps;
  std::size_t index{0};
  float elapsed{0.0F};
  float start_delay{0.0F};
  bool loop{true};

  std::size_t loop_from{0};
  bool finished{false};
  bool active{false};
  float phase{0.0F};
  std::uint8_t current_move{0};
  float applied_travel_x{0.0F};
  float applied_travel_z{0.0F};
  float facing_sin{0.0F};
  float facing_cos{1.0F};
  std::string armed_renderer_id;
  std::string released_renderer_id;
  bool has_throw_target{false};
  float throw_target_x{0.0F};
  float throw_target_z{0.0F};
  bool throw_armed{true};
};

class TargetCommitmentComponent {
public:
  TargetCommitmentComponent() = default;

  EntityID committed_target_id{0};
  float cooldown_remaining{0.0F};

  bool in_committed_phase{false};

  static constexpr float k_switch_cooldown = 0.8F;
};

class CohortMembershipComponent {
public:
  CohortMembershipComponent() = default;

  std::uint32_t cohort_id{0};
  bool cohort_activated{false};
};

class ElephantKnockbackCooldownComponent {
public:
  ElephantKnockbackCooldownComponent() = default;

  struct VictimCooldown {
    EntityID victim_id{0};
    float remaining{0.0F};
  };

  std::vector<VictimCooldown> cooldowns;
  static constexpr float k_knockback_cooldown = 1.0F;

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

[[nodiscard]] inline auto owner_id_of(const Entity* entity) -> int {
  const auto* unit =
      entity != nullptr ? entity->get_component<UnitComponent>() : nullptr;
  return unit != nullptr ? unit->owner_id : 0;
}

[[nodiscard]] inline auto owner_id_of(const Entity& entity) -> int {
  return owner_id_of(&entity);
}

} // namespace Engine::Core
