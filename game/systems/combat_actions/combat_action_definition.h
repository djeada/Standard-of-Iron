#pragma once

#include <cstdint>
#include <span>

#include "../../../animation/clip_manifest.h"
#include "../../core/component.h"

namespace Game::Systems::CombatActions {

enum class CombatActionId : std::uint8_t {
  None = 0,
  RpgSwordSlashLeft,
  RpgSwordSlashRight,
  RpgSwordOverhead,
  RpgSwordThrust,
  RpgSwordFinisher,
  CommanderSwordSpin,
  CommanderSwordLauncher,
  CommanderSwordGapCloser,
  CommanderSwordAirLight,
  CommanderSwordAirReverse,
  CommanderSwordDive,
  RpgSpearThrust,
  CommanderSpearStepThrust,
  RpgSpearSweep,
  RpgSpearFinisher,
  CommanderSpearLauncher,
  CommanderSpearGapCloser,
  CommanderSpearAirThrust,
  CommanderSpearDive,
  RpgBowShot,
  CommanderBowPowerShot,
  CommanderBowEvasiveShot,
  MountedSwordSlash,
  MountedSpearThrust,
  MountedChargeImpact,
  RtsSwordStrike,
  RtsSpearThrust,
  RtsBowShot,
  RtsElephantStomp,

  RtsCommanderThrust,
  RtsCommanderCut,
  RtsCommanderShot,

  RtsHeavyOverhead,
};

enum class WeaponFamily : std::uint8_t {
  None = 0,
  Sword,
  Spear,
  Bow,
  Shield,
  Mount,
  Body,
};

enum class CombatActionEventType : std::uint8_t {
  WindupStart,
  ActiveStart,
  WeaponTraceStart,
  WeaponTraceEnd,
  ProjectileRelease,
  RecoveryStart,
  CancelWindowStart,
  CancelWindowEnd,
  ExitSafe,
};

struct DamageProfile {
  float base_multiplier{1.0F};
  float posture_damage{0.0F};
  float guard_pressure{0.0F};

  bool unblockable{false};
};

struct HitShapeProfile {
  float reach{1.5F};
  float radius{0.35F};
};

enum class ActionLocomotionRequirement : std::uint8_t {
  Any = 0,
  Grounded,
  Airborne,
};

enum class CommanderActionRole : std::uint8_t {
  Routine = 0,
  Finisher,
  Launcher,
  GapCloser,
  Aerial,
  Dive,
  Special,
};

struct MovementProfile {
  float distance{0.0F};
  float start_normalized{0.0F};
  float end_normalized{0.0F};
};

struct TargetAssistProfile {
  float acquisition_range{0.0F};
  float cone_degrees{0.0F};
  float maximum_turn_degrees{0.0F};
  float magnetism_distance{0.0F};
};

struct ReactionProfile {
  Engine::Core::StaggerTier stagger_tier{Engine::Core::StaggerTier::LightFlinch};
  float stagger_seconds{0.0F};
  float launch_impulse{0.0F};
  float radial_radius{0.0F};
};

struct CombatActionEvent {
  CombatActionEventType type{CombatActionEventType::WindupStart};
  float normalized_time{0.0F};
};

struct CombatActionDefinition {
  CombatActionId id{CombatActionId::None};
  WeaponFamily weapon_family{WeaponFamily::None};
  Animation::SwordAttackAnimation sword_clip{
      Animation::SwordAttackAnimation::InfantrySlashA};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
  Engine::Core::AttackDirection attack_direction{
      Engine::Core::AttackDirection::LeftSlash};
  DamageProfile damage;
  HitShapeProfile hit_shape;
  float duration_seconds{1.8F};
  std::uint16_t rider_clip_id{Animation::k_unmapped_clip};
  std::span<const CombatActionEvent> events{};
  float light_stamina_cost{
      Engine::Core::CombatStateComponent::k_stamina_cost_light_attack};
  float heavy_stamina_cost{
      Engine::Core::CombatStateComponent::k_stamina_cost_heavy_attack};
  int max_targets{1};
  bool can_hit_same_target_once{true};
  bool requires_projectile_release{false};
  bool commander_only{false};
  ActionLocomotionRequirement locomotion{ActionLocomotionRequirement::Any};
  CommanderActionRole role{CommanderActionRole::Routine};
  CombatActionId next_light{CombatActionId::None};
  CombatActionId next_heavy{CombatActionId::None};
  CombatActionId next_jump{CombatActionId::None};
  MovementProfile movement{};
  TargetAssistProfile target_assist{};
  ReactionProfile reaction{};
};

[[nodiscard]] auto
resolve_commander_action(CombatActionId current_action,
                         Engine::Core::CommanderCombatIntentType intent,
                         WeaponFamily weapon,
                         bool airborne,
                         bool target_outside_normal_reach) noexcept -> CombatActionId;

[[nodiscard]] auto
all_combat_action_definitions() -> std::span<const CombatActionDefinition>;

[[nodiscard]] auto
find_combat_action_definition(CombatActionId id) -> const CombatActionDefinition*;

[[nodiscard]] auto
action_event_normalized_time(const CombatActionDefinition& definition,
                             CombatActionEventType type,
                             float fallback) -> float;

[[nodiscard]] auto
authored_phase_duration(const CombatActionDefinition& definition,
                        Engine::Core::CombatAnimationState state) -> float;

enum class MeleePhase : std::uint8_t {
  Ready = 0,
  Windup,
  EarlyStrike,
  CommittedStrike,
  FollowThrough
};

struct MeleeInterruption {
  MeleePhase phase{MeleePhase::Ready};

  bool accepts_attack{true};

  bool accepts_guard{true};
  bool accepts_dodge{true};

  float redirect_authority{1.0F};
};

inline constexpr float k_melee_recall_share = 0.35F;

[[nodiscard]] auto
melee_interruption_at(const CombatActionDefinition& definition,
                      float normalized_time) noexcept -> MeleeInterruption;

} // namespace Game::Systems::CombatActions
