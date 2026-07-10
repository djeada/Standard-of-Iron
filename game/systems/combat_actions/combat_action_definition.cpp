#include "combat_action_definition.h"

#include <array>

namespace Game::Systems::CombatActions {

namespace {

constexpr std::array<CombatActionEvent, 5> k_rpg_slash_events{{
    {CombatActionEventType::WindupStart, 0.09F},
    {CombatActionEventType::WeaponTraceStart, 0.38F},
    {CombatActionEventType::WeaponTraceEnd, 0.58F},
    {CombatActionEventType::RecoveryStart, 0.74F},
    {CombatActionEventType::ExitSafe, 0.92F},
}};

constexpr std::array<CombatActionEvent, 5> k_rpg_overhead_events{{
    {CombatActionEventType::WindupStart, 0.10F},
    {CombatActionEventType::WeaponTraceStart, 0.44F},
    {CombatActionEventType::WeaponTraceEnd, 0.60F},
    {CombatActionEventType::RecoveryStart, 0.80F},
    {CombatActionEventType::ExitSafe, 0.94F},
}};

constexpr std::array<CombatActionEvent, 5> k_rpg_thrust_events{{
    {CombatActionEventType::WindupStart, 0.08F},
    {CombatActionEventType::WeaponTraceStart, 0.34F},
    {CombatActionEventType::WeaponTraceEnd, 0.50F},
    {CombatActionEventType::RecoveryStart, 0.70F},
    {CombatActionEventType::ExitSafe, 0.90F},
}};

constexpr std::array<CombatActionEvent, 5> k_rpg_finisher_events{{
    {CombatActionEventType::WindupStart, 0.06F},
    {CombatActionEventType::WeaponTraceStart, 0.50F},
    {CombatActionEventType::WeaponTraceEnd, 0.68F},
    {CombatActionEventType::RecoveryStart, 0.84F},
    {CombatActionEventType::ExitSafe, 0.96F},
}};

constexpr std::array<CombatActionEvent, 5> k_rpg_spear_thrust_events{{
    {CombatActionEventType::WindupStart, 0.10F},
    {CombatActionEventType::WeaponTraceStart, 0.32F},
    {CombatActionEventType::WeaponTraceEnd, 0.52F},
    {CombatActionEventType::RecoveryStart, 0.72F},
    {CombatActionEventType::ExitSafe, 0.92F},
}};

constexpr std::array<CombatActionEvent, 5> k_rpg_spear_sweep_events{{
    {CombatActionEventType::WindupStart, 0.12F},
    {CombatActionEventType::WeaponTraceStart, 0.40F},
    {CombatActionEventType::WeaponTraceEnd, 0.62F},
    {CombatActionEventType::RecoveryStart, 0.80F},
    {CombatActionEventType::ExitSafe, 0.94F},
}};

constexpr std::array<CombatActionEvent, 5> k_rpg_bow_shot_events{{
    {CombatActionEventType::WindupStart, 0.08F},
    {CombatActionEventType::ActiveStart, 0.30F},
    {CombatActionEventType::ProjectileRelease, 0.46F},
    {CombatActionEventType::RecoveryStart, 0.70F},
    {CombatActionEventType::ExitSafe, 0.90F},
}};

constexpr std::array<CombatActionEvent, 5> k_mounted_sword_slash_events{{
    {CombatActionEventType::WindupStart, 0.08F},
    {CombatActionEventType::WeaponTraceStart, 0.30F},
    {CombatActionEventType::WeaponTraceEnd, 0.54F},
    {CombatActionEventType::RecoveryStart, 0.76F},
    {CombatActionEventType::ExitSafe, 0.92F},
}};

constexpr std::array<CombatActionEvent, 5> k_mounted_spear_thrust_events{{
    {CombatActionEventType::WindupStart, 0.08F},
    {CombatActionEventType::WeaponTraceStart, 0.26F},
    {CombatActionEventType::WeaponTraceEnd, 0.48F},
    {CombatActionEventType::RecoveryStart, 0.72F},
    {CombatActionEventType::ExitSafe, 0.90F},
}};

constexpr std::array<CombatActionEvent, 4> k_mounted_charge_impact_events{{
    {CombatActionEventType::WindupStart, 0.05F},
    {CombatActionEventType::ActiveStart, 0.20F},
    {CombatActionEventType::RecoveryStart, 0.72F},
    {CombatActionEventType::ExitSafe, 0.90F},
}};

constexpr std::array<CombatActionDefinition, 11> k_definitions{{
    {
        .id = CombatActionId::RpgSwordSlashLeft,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgSlashLeft,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::LeftSlash,
        .damage = {.base_multiplier = 1.0F,
                   .posture_damage = 8.0F,
                   .guard_pressure = 12.0F},
        .hit_shape = {.reach = 1.8F, .radius = 0.42F},
        .duration_seconds = 2.35F,
        .events = k_rpg_slash_events,
    },
    {
        .id = CombatActionId::RpgSwordSlashRight,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgSlashRight,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::RightSlash,
        .damage = {.base_multiplier = 1.0F,
                   .posture_damage = 8.0F,
                   .guard_pressure = 12.0F},
        .hit_shape = {.reach = 1.8F, .radius = 0.42F},
        .duration_seconds = 2.35F,
        .events = k_rpg_slash_events,
    },
    {
        .id = CombatActionId::RpgSwordOverhead,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgOverhead,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::Overhead,
        .damage = {.base_multiplier = 1.0F,
                   .posture_damage = 12.0F,
                   .guard_pressure = 16.0F},
        .hit_shape = {.reach = 1.9F, .radius = 0.36F},
        .duration_seconds = 2.65F,
        .events = k_rpg_overhead_events,
    },
    {
        .id = CombatActionId::RpgSwordThrust,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgThrust,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.0F,
                   .posture_damage = 10.0F,
                   .guard_pressure = 14.0F},
        .hit_shape = {.reach = 2.05F, .radius = 0.28F},
        .duration_seconds = 1.90F,
        .events = k_rpg_thrust_events,
    },
    {
        .id = CombatActionId::RpgSwordFinisher,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgFinisher,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::HeavyOverhead,
        .damage = {.base_multiplier = 1.5F,
                   .posture_damage = 18.0F,
                   .guard_pressure = 30.0F},
        .hit_shape = {.reach = 2.0F, .radius = 0.48F},
        .duration_seconds = 3.40F,
        .events = k_rpg_finisher_events,
        .max_targets = 2,
    },
    {
        .id = CombatActionId::RpgSpearThrust,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.05F,
                   .posture_damage = 14.0F,
                   .guard_pressure = 18.0F},
        .hit_shape = {.reach = 2.75F, .radius = 0.16F},
        .duration_seconds = 1.95F,
        .events = k_rpg_spear_thrust_events,
    },
    {
        .id = CombatActionId::RpgSpearSweep,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::LeftSlash,
        .damage = {.base_multiplier = 0.95F,
                   .posture_damage = 10.0F,
                   .guard_pressure = 14.0F},
        .hit_shape = {.reach = 2.35F, .radius = 0.34F},
        .duration_seconds = 2.35F,
        .events = k_rpg_spear_sweep_events,
        .max_targets = 2,
    },
    {
        .id = CombatActionId::RpgBowShot,
        .weapon_family = WeaponFamily::Bow,
        .attack_family = Engine::Core::CombatAttackFamily::Bow,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.0F,
                   .posture_damage = 4.0F,
                   .guard_pressure = 6.0F},
        .hit_shape = {.reach = 12.0F, .radius = 0.10F},
        .duration_seconds = 2.10F,
        .events = k_rpg_bow_shot_events,
        .requires_projectile_release = true,
    },
    {
        .id = CombatActionId::MountedSwordSlash,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgSlashRight,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::RightSlash,
        .damage = {.base_multiplier = 1.15F,
                   .posture_damage = 14.0F,
                   .guard_pressure = 20.0F},
        .hit_shape = {.reach = 2.25F, .radius = 0.50F},
        .duration_seconds = 1.65F,
        .rider_clip_id = Animation::k_humanoid_riding_sword_strike_clip,
        .events = k_mounted_sword_slash_events,
        .max_targets = 2,
    },
    {
        .id = CombatActionId::MountedSpearThrust,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.10F,
                   .posture_damage = 18.0F,
                   .guard_pressure = 24.0F},
        .hit_shape = {.reach = 3.15F, .radius = 0.20F},
        .duration_seconds = 1.70F,
        .rider_clip_id = Animation::k_humanoid_riding_spear_thrust_clip,
        .events = k_mounted_spear_thrust_events,
        .max_targets = 2,
    },
    {
        .id = CombatActionId::MountedChargeImpact,
        .weapon_family = WeaponFamily::Mount,
        .attack_family = Engine::Core::CombatAttackFamily::None,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.25F,
                   .posture_damage = 22.0F,
                   .guard_pressure = 30.0F},
        .hit_shape = {.reach = 1.35F, .radius = 0.75F},
        .duration_seconds = 1.40F,
        .rider_clip_id = Animation::k_humanoid_riding_charge_clip,
        .events = k_mounted_charge_impact_events,
        .max_targets = 3,
    },
}};

} // namespace

auto all_combat_action_definitions() -> std::span<const CombatActionDefinition> {
  return k_definitions;
}

auto find_combat_action_definition(CombatActionId id) -> const CombatActionDefinition* {
  for (auto const& definition : k_definitions) {
    if (definition.id == id) {
      return &definition;
    }
  }
  return nullptr;
}

} // namespace Game::Systems::CombatActions
