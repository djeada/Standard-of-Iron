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
  RpgSpearThrust,
  RpgSpearSweep,
  RpgBowShot,
  MountedSwordSlash,
  MountedSpearThrust,
  MountedChargeImpact,
  RtsSwordStrike,
  RtsSpearThrust,
  RtsBowShot,
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
};

struct HitShapeProfile {
  float reach{1.5F};
  float radius{0.35F};
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
};

[[nodiscard]] auto
all_combat_action_definitions() -> std::span<const CombatActionDefinition>;

[[nodiscard]] auto
find_combat_action_definition(CombatActionId id) -> const CombatActionDefinition*;

} // namespace Game::Systems::CombatActions
