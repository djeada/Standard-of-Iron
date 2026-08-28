#include "combat_action_definition.h"

#include <algorithm>
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

constexpr std::array<CombatActionEvent, 5> k_rpg_spear_finisher_events{{
    {CombatActionEventType::WindupStart, 0.08F},
    {CombatActionEventType::WeaponTraceStart, 0.48F},
    {CombatActionEventType::WeaponTraceEnd, 0.66F},
    {CombatActionEventType::RecoveryStart, 0.82F},
    {CombatActionEventType::ExitSafe, 0.96F},
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

constexpr std::array<CombatActionEvent, 6> k_rts_melee_events{{
    {CombatActionEventType::WindupStart, 0.08F},
    {CombatActionEventType::ActiveStart, 0.40F},
    {CombatActionEventType::WeaponTraceStart, 0.40F},
    {CombatActionEventType::WeaponTraceEnd, 0.56F},
    {CombatActionEventType::RecoveryStart, 0.72F},
    {CombatActionEventType::ExitSafe, 0.92F},
}};

constexpr std::array<CombatActionEvent, 6> k_rts_heavy_overhead_events{{
    {CombatActionEventType::WindupStart, 0.05F},
    {CombatActionEventType::ActiveStart, 0.56F},
    {CombatActionEventType::WeaponTraceStart, 0.56F},
    {CombatActionEventType::WeaponTraceEnd, 0.70F},
    {CombatActionEventType::RecoveryStart, 0.82F},
    {CombatActionEventType::ExitSafe, 0.96F},
}};

constexpr std::array<CombatActionEvent, 5> k_rts_bow_events{{
    {CombatActionEventType::WindupStart, 0.08F},
    {CombatActionEventType::ActiveStart, 0.30F},
    {CombatActionEventType::ProjectileRelease, 0.46F},
    {CombatActionEventType::RecoveryStart, 0.70F},
    {CombatActionEventType::ExitSafe, 0.90F},
}};

constexpr std::array<CombatActionEvent, 5> k_rts_elephant_stomp_events{{
    {CombatActionEventType::WindupStart, 0.06F},
    {CombatActionEventType::ActiveStart, 0.46F},
    {CombatActionEventType::WeaponTraceEnd, 0.58F},
    {CombatActionEventType::RecoveryStart, 0.72F},
    {CombatActionEventType::ExitSafe, 0.92F},
}};

constexpr std::array<CombatActionDefinition, 33> k_definitions{{
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
        .duration_seconds = 0.72F,
        .events = k_rpg_slash_events,
        .max_targets = 2,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .next_light = CombatActionId::RpgSwordSlashRight,
        .next_heavy = CombatActionId::CommanderSwordLauncher,
        .next_jump = CombatActionId::CommanderSwordAirLight,
        .movement = {.distance = 0.65F,
                     .start_normalized = 0.08F,
                     .end_normalized = 0.58F},
        .target_assist = {.acquisition_range = 2.6F,
                          .cone_degrees = 55.0F,
                          .maximum_turn_degrees = 30.0F,
                          .magnetism_distance = 0.45F},
        .reaction = {.stagger_seconds = 0.12F},
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
        .duration_seconds = 0.72F,
        .events = k_rpg_slash_events,
        .max_targets = 2,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .next_light = CombatActionId::CommanderSwordSpin,
        .next_heavy = CombatActionId::RpgSwordFinisher,
        .next_jump = CombatActionId::CommanderSwordAirLight,
        .movement = {.distance = 0.8F,
                     .start_normalized = 0.08F,
                     .end_normalized = 0.58F},
        .target_assist = {.acquisition_range = 2.8F,
                          .cone_degrees = 60.0F,
                          .maximum_turn_degrees = 32.0F,
                          .magnetism_distance = 0.5F},
        .reaction = {.stagger_seconds = 0.14F},
    },
    {
        .id = CombatActionId::RpgSwordOverhead,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgOverhead,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::Overhead,
        .damage = {.base_multiplier = 1.4F,
                   .posture_damage = 12.0F,
                   .guard_pressure = 16.0F},
        .hit_shape = {.reach = 1.9F, .radius = 0.36F},
        .duration_seconds = 0.88F,
        .events = k_rpg_overhead_events,
        .max_targets = 3,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::Finisher,
        .movement = {.distance = 1.15F,
                     .start_normalized = 0.1F,
                     .end_normalized = 0.6F},
        .target_assist = {.acquisition_range = 3.2F,
                          .cone_degrees = 42.0F,
                          .maximum_turn_degrees = 22.0F,
                          .magnetism_distance = 0.75F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.42F},
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
        .duration_seconds = 0.60F,
        .events = k_rpg_thrust_events,
        .max_targets = 2,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .next_light = CombatActionId::RpgSwordSlashLeft,
        .next_heavy = CombatActionId::CommanderSwordLauncher,
        .movement = {.distance = 1.1F,
                     .start_normalized = 0.06F,
                     .end_normalized = 0.5F},
        .target_assist = {.acquisition_range = 3.1F,
                          .cone_degrees = 36.0F,
                          .maximum_turn_degrees = 34.0F,
                          .magnetism_distance = 0.8F},
        .reaction = {.stagger_seconds = 0.16F},
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
        .duration_seconds = 1.15F,
        .events = k_rpg_finisher_events,
        .max_targets = 5,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::Finisher,
        .movement = {.distance = 1.35F,
                     .start_normalized = 0.08F,
                     .end_normalized = 0.68F},
        .target_assist = {.acquisition_range = 3.4F,
                          .cone_degrees = 65.0F,
                          .maximum_turn_degrees = 28.0F,
                          .magnetism_distance = 0.9F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::Knockback,
                     .stagger_seconds = 0.75F,
                     .radial_radius = 2.4F},
    },
    {
        .id = CombatActionId::CommanderSwordSpin,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgSlashRight,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::RightSlash,
        .damage = {.base_multiplier = 1.25F,
                   .posture_damage = 16.0F,
                   .guard_pressure = 22.0F},
        .hit_shape = {.reach = 2.35F, .radius = 0.72F},
        .duration_seconds = 0.92F,
        .events = k_rpg_spear_sweep_events,
        .max_targets = 6,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::Finisher,
        .next_light = CombatActionId::RpgSwordFinisher,
        .next_heavy = CombatActionId::RpgSwordFinisher,
        .next_jump = CombatActionId::CommanderSwordAirLight,
        .movement = {.distance = 0.45F,
                     .start_normalized = 0.1F,
                     .end_normalized = 0.7F},
        .target_assist = {.acquisition_range = 3.0F,
                          .cone_degrees = 120.0F,
                          .maximum_turn_degrees = 18.0F,
                          .magnetism_distance = 0.35F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.38F,
                     .radial_radius = 2.6F},
    },
    {
        .id = CombatActionId::CommanderSwordLauncher,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgOverhead,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::Overhead,
        .damage = {.base_multiplier = 1.2F,
                   .posture_damage = 24.0F,
                   .guard_pressure = 28.0F},
        .hit_shape = {.reach = 2.1F, .radius = 0.46F},
        .duration_seconds = 0.86F,
        .events = k_rpg_overhead_events,
        .max_targets = 3,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::Launcher,
        .next_jump = CombatActionId::CommanderSwordAirLight,
        .movement = {.distance = 0.6F,
                     .start_normalized = 0.08F,
                     .end_normalized = 0.58F},
        .target_assist = {.acquisition_range = 2.8F,
                          .cone_degrees = 48.0F,
                          .maximum_turn_degrees = 32.0F,
                          .magnetism_distance = 0.6F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::Knockback,
                     .stagger_seconds = 0.65F,
                     .launch_impulse = 6.5F},
    },
    {
        .id = CombatActionId::CommanderSwordGapCloser,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgThrust,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.35F,
                   .posture_damage = 18.0F,
                   .guard_pressure = 24.0F},
        .hit_shape = {.reach = 2.4F, .radius = 0.34F},
        .duration_seconds = 0.78F,
        .events = k_rpg_thrust_events,
        .max_targets = 3,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::GapCloser,
        .next_light = CombatActionId::RpgSwordSlashLeft,
        .next_heavy = CombatActionId::RpgSwordFinisher,
        .movement = {.distance = 2.7F,
                     .start_normalized = 0.02F,
                     .end_normalized = 0.5F},
        .target_assist = {.acquisition_range = 5.0F,
                          .cone_degrees = 42.0F,
                          .maximum_turn_degrees = 40.0F,
                          .magnetism_distance = 1.2F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.4F},
    },
    {
        .id = CombatActionId::CommanderSwordAirLight,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgSlashLeft,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::LeftSlash,
        .damage = {.base_multiplier = 1.15F,
                   .posture_damage = 14.0F,
                   .guard_pressure = 16.0F},
        .hit_shape = {.reach = 2.1F, .radius = 0.55F},
        .duration_seconds = 0.62F,
        .events = k_rpg_slash_events,
        .max_targets = 4,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Airborne,
        .role = CommanderActionRole::Aerial,
        .next_light = CombatActionId::CommanderSwordAirReverse,
        .next_heavy = CombatActionId::CommanderSwordDive,
        .movement = {.distance = 0.7F,
                     .start_normalized = 0.08F,
                     .end_normalized = 0.58F},
        .target_assist = {.acquisition_range = 3.0F,
                          .cone_degrees = 70.0F,
                          .maximum_turn_degrees = 35.0F,
                          .magnetism_distance = 0.7F},
        .reaction = {.stagger_seconds = 0.2F},
    },
    {
        .id = CombatActionId::CommanderSwordAirReverse,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgSlashRight,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::RightSlash,
        .damage = {.base_multiplier = 1.2F,
                   .posture_damage = 16.0F,
                   .guard_pressure = 18.0F},
        .hit_shape = {.reach = 2.2F, .radius = 0.58F},
        .duration_seconds = 0.68F,
        .events = k_rpg_slash_events,
        .max_targets = 4,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Airborne,
        .role = CommanderActionRole::Aerial,
        .next_heavy = CombatActionId::CommanderSwordDive,
        .target_assist = {.acquisition_range = 3.0F,
                          .cone_degrees = 80.0F,
                          .maximum_turn_degrees = 32.0F,
                          .magnetism_distance = 0.65F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.3F},
    },
    {
        .id = CombatActionId::CommanderSwordDive,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgFinisher,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::HeavyOverhead,
        .damage = {.base_multiplier = 2.0F,
                   .posture_damage = 42.0F,
                   .guard_pressure = 52.0F,
                   .unblockable = true},
        .hit_shape = {.reach = 2.4F, .radius = 1.0F},
        .duration_seconds = 0.95F,
        .events = k_rpg_finisher_events,
        .max_targets = 8,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Airborne,
        .role = CommanderActionRole::Dive,
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::Knockdown,
                     .stagger_seconds = 0.9F,
                     .launch_impulse = 4.0F,
                     .radial_radius = 3.1F},
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
        .duration_seconds = 0.64F,
        .events = k_rpg_spear_thrust_events,
        .max_targets = 2,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .next_light = CombatActionId::CommanderSpearStepThrust,
        .next_heavy = CombatActionId::RpgSpearSweep,
        .next_jump = CombatActionId::CommanderSpearAirThrust,
        .movement = {.distance = 0.4F,
                     .start_normalized = 0.08F,
                     .end_normalized = 0.52F},
        .target_assist = {.acquisition_range = 3.5F,
                          .cone_degrees = 32.0F,
                          .maximum_turn_degrees = 38.0F,
                          .magnetism_distance = 0.65F},
        .reaction = {.stagger_seconds = 0.14F},
    },
    {
        .id = CombatActionId::CommanderSpearStepThrust,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.15F,
                   .posture_damage = 16.0F,
                   .guard_pressure = 20.0F},
        .hit_shape = {.reach = 3.0F, .radius = 0.18F},
        .duration_seconds = 0.7F,
        .events = k_rpg_spear_thrust_events,
        .max_targets = 2,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .next_light = CombatActionId::RpgSpearSweep,
        .next_heavy = CombatActionId::CommanderSpearLauncher,
        .next_jump = CombatActionId::CommanderSpearAirThrust,
        .movement = {.distance = 1.0F,
                     .start_normalized = 0.06F,
                     .end_normalized = 0.52F},
        .target_assist = {.acquisition_range = 3.8F,
                          .cone_degrees = 30.0F,
                          .maximum_turn_degrees = 40.0F,
                          .magnetism_distance = 0.8F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.25F},
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
        .duration_seconds = 0.82F,
        .events = k_rpg_spear_sweep_events,
        .max_targets = 6,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .next_light = CombatActionId::RpgSpearFinisher,
        .next_heavy = CombatActionId::RpgSpearFinisher,
        .next_jump = CombatActionId::CommanderSpearAirThrust,
        .movement = {.distance = 0.45F,
                     .start_normalized = 0.1F,
                     .end_normalized = 0.65F},
        .target_assist = {.acquisition_range = 3.5F,
                          .cone_degrees = 100.0F,
                          .maximum_turn_degrees = 24.0F,
                          .magnetism_distance = 0.4F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.38F,
                     .radial_radius = 2.9F},
    },
    {
        .id = CombatActionId::RpgSpearFinisher,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.5F,
                   .posture_damage = 18.0F,
                   .guard_pressure = 28.0F},
        .hit_shape = {.reach = 3.05F, .radius = 0.22F},
        .duration_seconds = 1.05F,
        .events = k_rpg_spear_finisher_events,
        .max_targets = 4,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::Finisher,
        .movement = {.distance = 1.6F,
                     .start_normalized = 0.06F,
                     .end_normalized = 0.66F},
        .target_assist = {.acquisition_range = 4.4F,
                          .cone_degrees = 28.0F,
                          .maximum_turn_degrees = 36.0F,
                          .magnetism_distance = 1.0F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::Knockback,
                     .stagger_seconds = 0.72F,
                     .launch_impulse = 2.4F},
    },
    {
        .id = CombatActionId::CommanderSpearLauncher,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Overhead,
        .damage = {.base_multiplier = 1.3F,
                   .posture_damage = 28.0F,
                   .guard_pressure = 30.0F},
        .hit_shape = {.reach = 2.8F, .radius = 0.32F},
        .duration_seconds = 0.88F,
        .events = k_rpg_spear_sweep_events,
        .max_targets = 3,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::Launcher,
        .next_jump = CombatActionId::CommanderSpearAirThrust,
        .movement = {.distance = 0.55F,
                     .start_normalized = 0.08F,
                     .end_normalized = 0.58F},
        .target_assist = {.acquisition_range = 3.6F,
                          .cone_degrees = 38.0F,
                          .maximum_turn_degrees = 38.0F,
                          .magnetism_distance = 0.75F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::Knockback,
                     .stagger_seconds = 0.7F,
                     .launch_impulse = 7.0F},
    },
    {
        .id = CombatActionId::CommanderSpearGapCloser,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.45F,
                   .posture_damage = 24.0F,
                   .guard_pressure = 32.0F},
        .hit_shape = {.reach = 3.35F, .radius = 0.2F},
        .duration_seconds = 0.82F,
        .events = k_rpg_spear_thrust_events,
        .max_targets = 3,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Grounded,
        .role = CommanderActionRole::GapCloser,
        .next_light = CombatActionId::RpgSpearThrust,
        .next_heavy = CombatActionId::RpgSpearFinisher,
        .movement = {.distance = 3.0F,
                     .start_normalized = 0.02F,
                     .end_normalized = 0.5F},
        .target_assist = {.acquisition_range = 6.0F,
                          .cone_degrees = 30.0F,
                          .maximum_turn_degrees = 44.0F,
                          .magnetism_distance = 1.35F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.5F},
    },
    {
        .id = CombatActionId::CommanderSpearAirThrust,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.25F,
                   .posture_damage = 18.0F,
                   .guard_pressure = 22.0F},
        .hit_shape = {.reach = 3.0F, .radius = 0.24F},
        .duration_seconds = 0.68F,
        .events = k_rpg_spear_thrust_events,
        .max_targets = 3,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Airborne,
        .role = CommanderActionRole::Aerial,
        .next_heavy = CombatActionId::CommanderSpearDive,
        .movement = {.distance = 0.85F,
                     .start_normalized = 0.05F,
                     .end_normalized = 0.54F},
        .target_assist = {.acquisition_range = 4.0F,
                          .cone_degrees = 36.0F,
                          .maximum_turn_degrees = 42.0F,
                          .magnetism_distance = 0.9F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.32F},
    },
    {
        .id = CombatActionId::CommanderSpearDive,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::HeavyOverhead,
        .damage = {.base_multiplier = 2.15F,
                   .posture_damage = 48.0F,
                   .guard_pressure = 58.0F,
                   .unblockable = true},
        .hit_shape = {.reach = 3.2F, .radius = 0.7F},
        .duration_seconds = 0.92F,
        .events = k_rpg_spear_finisher_events,
        .max_targets = 8,
        .commander_only = true,
        .locomotion = ActionLocomotionRequirement::Airborne,
        .role = CommanderActionRole::Dive,
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::Knockdown,
                     .stagger_seconds = 1.0F,
                     .launch_impulse = 4.5F,
                     .radial_radius = 3.3F},
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
        .duration_seconds = 1.05F,
        .events = k_rpg_bow_shot_events,
        .requires_projectile_release = true,
        .commander_only = true,
        .role = CommanderActionRole::Routine,
        .next_heavy = CombatActionId::CommanderBowPowerShot,
        .target_assist = {.acquisition_range = 16.0F,
                          .cone_degrees = 24.0F,
                          .maximum_turn_degrees = 16.0F},
    },
    {
        .id = CombatActionId::CommanderBowPowerShot,
        .weapon_family = WeaponFamily::Bow,
        .attack_family = Engine::Core::CombatAttackFamily::Bow,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 2.0F,
                   .posture_damage = 26.0F,
                   .guard_pressure = 38.0F},
        .hit_shape = {.reach = 18.0F, .radius = 0.12F},
        .duration_seconds = 1.3F,
        .events = k_rpg_bow_shot_events,
        .heavy_stamina_cost = 22.0F,
        .max_targets = 2,
        .requires_projectile_release = true,
        .commander_only = true,
        .role = CommanderActionRole::Finisher,
        .target_assist = {.acquisition_range = 20.0F,
                          .cone_degrees = 18.0F,
                          .maximum_turn_degrees = 12.0F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::Knockback,
                     .stagger_seconds = 0.6F},
    },
    {
        .id = CombatActionId::CommanderBowEvasiveShot,
        .weapon_family = WeaponFamily::Bow,
        .attack_family = Engine::Core::CombatAttackFamily::Bow,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .damage = {.base_multiplier = 1.35F,
                   .posture_damage = 14.0F,
                   .guard_pressure = 18.0F},
        .hit_shape = {.reach = 14.0F, .radius = 0.1F},
        .duration_seconds = 0.78F,
        .events = k_rpg_bow_shot_events,
        .max_targets = 2,
        .requires_projectile_release = true,
        .commander_only = true,
        .role = CommanderActionRole::Special,
        .movement = {.distance = -1.8F,
                     .start_normalized = 0.02F,
                     .end_normalized = 0.45F},
        .target_assist = {.acquisition_range = 16.0F,
                          .cone_degrees = 35.0F,
                          .maximum_turn_degrees = 22.0F},
        .reaction = {.stagger_tier = Engine::Core::StaggerTier::HeavyStagger,
                     .stagger_seconds = 0.3F},
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
        .duration_seconds = 0.95F,
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
        .duration_seconds = 0.90F,
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
    {
        .id = CombatActionId::RtsSwordStrike,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::InfantrySlashA,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::LeftSlash,
        .hit_shape = {.reach = 1.8F, .radius = 0.35F},
        .duration_seconds = 1.0F,
        .events = k_rts_melee_events,
    },
    {
        .id = CombatActionId::RtsHeavyOverhead,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::RpgOverhead,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::HeavyOverhead,
        .damage = {.base_multiplier = 1.6F,
                   .posture_damage = 26.0F,
                   .guard_pressure = 34.0F,
                   .unblockable = true},
        .hit_shape = {.reach = 2.0F, .radius = 0.44F},
        .duration_seconds = 1.55F,
        .events = k_rts_heavy_overhead_events,
    },
    {
        .id = CombatActionId::RtsSpearThrust,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .hit_shape = {.reach = 2.6F, .radius = 0.18F},
        .duration_seconds = 1.0F,
        .events = k_rts_melee_events,
    },
    {
        .id = CombatActionId::RtsBowShot,
        .weapon_family = WeaponFamily::Bow,
        .attack_family = Engine::Core::CombatAttackFamily::Bow,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .hit_shape = {.reach = 12.0F, .radius = 0.10F},
        .duration_seconds = 1.0F,
        .events = k_rts_bow_events,
        .requires_projectile_release = true,
    },
    {
        .id = CombatActionId::RtsCommanderThrust,
        .weapon_family = WeaponFamily::Spear,
        .attack_family = Engine::Core::CombatAttackFamily::Spear,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .hit_shape = {.reach = 3.1F, .radius = 0.9F},
        .duration_seconds = 1.25F,
        .events = k_rts_melee_events,
        .max_targets = 4,
    },
    {
        .id = CombatActionId::RtsCommanderCut,
        .weapon_family = WeaponFamily::Sword,
        .sword_clip = Animation::SwordAttackAnimation::InfantrySlashC,
        .attack_family = Engine::Core::CombatAttackFamily::Sword,
        .attack_direction = Engine::Core::AttackDirection::HeavyOverhead,
        .hit_shape = {.reach = 2.1F, .radius = 0.55F},
        .duration_seconds = 1.15F,
        .events = k_rts_melee_events,
        .max_targets = 2,
    },
    {
        .id = CombatActionId::RtsCommanderShot,
        .weapon_family = WeaponFamily::Bow,
        .attack_family = Engine::Core::CombatAttackFamily::Bow,
        .attack_direction = Engine::Core::AttackDirection::Thrust,
        .hit_shape = {.reach = 6.0F, .radius = 0.10F},
        .duration_seconds = 0.85F,
        .events = k_rts_bow_events,
        .requires_projectile_release = true,
    },
    {
        .id = CombatActionId::RtsElephantStomp,
        .weapon_family = WeaponFamily::Body,
        .attack_family = Engine::Core::CombatAttackFamily::None,
        .attack_direction = Engine::Core::AttackDirection::HeavyOverhead,
        .hit_shape = {.reach = 3.0F, .radius = 2.5F},
        .duration_seconds = 1.15F,
        .events = k_rts_elephant_stomp_events,
        .max_targets = 8,
    },
}};

} // namespace

auto resolve_commander_action(CombatActionId current_action,
                              Engine::Core::CommanderCombatIntentType intent,
                              WeaponFamily weapon,
                              bool airborne,
                              bool target_outside_normal_reach) noexcept
    -> CombatActionId {
  auto const* current = find_combat_action_definition(current_action);
  if (current != nullptr && current->weapon_family == weapon) {
    CombatActionId branch = CombatActionId::None;
    switch (intent) {
    case Engine::Core::CommanderCombatIntentType::Light:
      branch = current->next_light;
      break;
    case Engine::Core::CommanderCombatIntentType::Heavy:
      branch = current->next_heavy;
      break;
    case Engine::Core::CommanderCombatIntentType::Jump:
      branch = current->next_jump;
      break;
    case Engine::Core::CommanderCombatIntentType::Dodge:
    case Engine::Core::CommanderCombatIntentType::Guard:
    case Engine::Core::CommanderCombatIntentType::Special:
      break;
    case Engine::Core::CommanderCombatIntentType::WeaponSwitch:
      return weapon == WeaponFamily::Spear
                 ? CombatActionId::RpgSpearSweep
                 : (weapon == WeaponFamily::Bow
                        ? CombatActionId::CommanderBowEvasiveShot
                        : CombatActionId::CommanderSwordSpin);
    }
    if (branch != CombatActionId::None) {
      return branch;
    }
  }

  if (airborne) {
    if (intent == Engine::Core::CommanderCombatIntentType::Heavy) {
      return weapon == WeaponFamily::Spear ? CombatActionId::CommanderSpearDive
                                           : CombatActionId::CommanderSwordDive;
    }
    if (intent == Engine::Core::CommanderCombatIntentType::Light) {
      return weapon == WeaponFamily::Spear
                 ? CombatActionId::CommanderSpearAirThrust
                 : (weapon == WeaponFamily::Sword
                        ? CombatActionId::CommanderSwordAirLight
                        : CombatActionId::RpgBowShot);
    }
  }

  if (intent == Engine::Core::CommanderCombatIntentType::Special) {
    switch (weapon) {
    case WeaponFamily::Sword:
      return CombatActionId::CommanderSwordSpin;
    case WeaponFamily::Spear:
      return CombatActionId::RpgSpearSweep;
    case WeaponFamily::Bow:
      return CombatActionId::CommanderBowEvasiveShot;
    default:
      return CombatActionId::None;
    }
  }

  if (intent == Engine::Core::CommanderCombatIntentType::WeaponSwitch) {
    return weapon == WeaponFamily::Spear
               ? CombatActionId::RpgSpearSweep
               : (weapon == WeaponFamily::Bow ? CombatActionId::CommanderBowEvasiveShot
                                              : CombatActionId::CommanderSwordSpin);
  }

  if (intent == Engine::Core::CommanderCombatIntentType::Heavy) {
    if (target_outside_normal_reach) {
      if (weapon == WeaponFamily::Sword) {
        return CombatActionId::CommanderSwordGapCloser;
      }
      if (weapon == WeaponFamily::Spear) {
        return CombatActionId::CommanderSpearGapCloser;
      }
    }
    switch (weapon) {
    case WeaponFamily::Sword:
      return CombatActionId::RpgSwordOverhead;
    case WeaponFamily::Spear:
      return CombatActionId::RpgSpearFinisher;
    case WeaponFamily::Bow:
      return CombatActionId::CommanderBowPowerShot;
    default:
      return CombatActionId::None;
    }
  }

  switch (weapon) {
  case WeaponFamily::Sword:
    return CombatActionId::RpgSwordSlashLeft;
  case WeaponFamily::Spear:
    return CombatActionId::RpgSpearThrust;
  case WeaponFamily::Bow:
    return CombatActionId::RpgBowShot;
  default:
    return CombatActionId::None;
  }
}

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

auto action_event_normalized_time(const CombatActionDefinition& definition,
                                  CombatActionEventType type,
                                  float fallback) -> float {
  for (auto const& event : definition.events) {
    if (event.type == type) {
      return event.normalized_time;
    }
  }
  return fallback;
}

auto authored_phase_duration(const CombatActionDefinition& definition,
                             Engine::Core::CombatAnimationState state) -> float {
  if (definition.duration_seconds <= 0.0F || definition.events.empty()) {
    return 0.0F;
  }

  float const windup = action_event_normalized_time(
      definition, CombatActionEventType::WindupStart, 0.08F);
  float const active = action_event_normalized_time(
      definition, CombatActionEventType::ActiveStart, 0.35F);
  float const strike = action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceStart, active);
  float const strike_end = action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceEnd, strike);
  float const recovery = action_event_normalized_time(
      definition, CombatActionEventType::RecoveryStart, 0.75F);
  float const exit_safe =
      action_event_normalized_time(definition, CombatActionEventType::ExitSafe, 0.92F);

  float span = 0.0F;
  switch (state) {
  case Engine::Core::CombatAnimationState::Advance:
    span = windup;
    break;
  case Engine::Core::CombatAnimationState::WindUp:
    span = strike - windup;
    break;
  case Engine::Core::CombatAnimationState::Strike:
    span = strike_end - strike;
    break;
  case Engine::Core::CombatAnimationState::Impact:
    span = recovery - strike_end;
    break;
  case Engine::Core::CombatAnimationState::Recover:
    span = exit_safe - recovery;
    break;
  case Engine::Core::CombatAnimationState::Reposition:
    span = 1.0F - exit_safe;
    break;
  case Engine::Core::CombatAnimationState::Idle:
  default:
    return 0.0F;
  }

  constexpr float k_min_phase_span = 0.01F;
  return std::max(span, k_min_phase_span) * definition.duration_seconds;
}

auto melee_interruption_at(const CombatActionDefinition& definition,
                           float normalized_time) noexcept -> MeleeInterruption {
  float const windup = action_event_normalized_time(
      definition, CombatActionEventType::WindupStart, 0.08F);
  float const active = action_event_normalized_time(
      definition, CombatActionEventType::ActiveStart, 0.35F);
  float const strike = action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceStart, active);
  float const strike_end = action_event_normalized_time(
      definition, CombatActionEventType::WeaponTraceEnd, strike);
  float const recovery = action_event_normalized_time(
      definition, CombatActionEventType::RecoveryStart, 0.75F);
  float const exit_safe =
      action_event_normalized_time(definition, CombatActionEventType::ExitSafe, 0.92F);

  float const t = std::clamp(normalized_time, 0.0F, 1.0F);
  if (t >= exit_safe) {
    return {.phase = MeleePhase::Ready};
  }

  if (t < strike) {

    return {.phase = t < windup ? MeleePhase::Ready : MeleePhase::Windup,
            .accepts_attack = true,
            .accepts_guard = true,
            .accepts_dodge = true,
            .redirect_authority = 1.0F};
  }

  float const recall_end = strike + ((strike_end - strike) * k_melee_recall_share);
  if (t < recall_end) {

    return {.phase = MeleePhase::EarlyStrike,
            .accepts_attack = false,
            .accepts_guard = false,
            .accepts_dodge = true,
            .redirect_authority = 0.35F};
  }

  if (t < strike_end) {
    return {.phase = MeleePhase::CommittedStrike,
            .accepts_attack = false,
            .accepts_guard = false,
            .accepts_dodge = false,
            .redirect_authority = 0.0F};
  }

  bool const settled = t >= recovery;
  return {.phase = MeleePhase::FollowThrough,
          .accepts_attack = settled,
          .accepts_guard = settled,
          .accepts_dodge = true,
          .redirect_authority = 0.0F};
}

} // namespace Game::Systems::CombatActions
