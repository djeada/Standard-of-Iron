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

constexpr std::array<CombatActionDefinition, 20> k_definitions{{
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
        .duration_seconds = 0.88F,
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
        .duration_seconds = 0.60F,
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
        .duration_seconds = 1.15F,
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
        .duration_seconds = 0.64F,
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
        .duration_seconds = 0.82F,
        .events = k_rpg_spear_sweep_events,
        .max_targets = 2,
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
        .duration_seconds = 1.05F,
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
