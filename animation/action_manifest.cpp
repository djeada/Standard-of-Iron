#include "action_manifest.h"

#include <algorithm>
#include <cmath>

namespace Animation {

auto wrap_action_phase(float phase) noexcept -> float {
  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto resolve_humanoid_action_sample(const HumanoidActionSampleInputs& inputs) noexcept
    -> HumanoidActionSample {
  HumanoidActionSample sample{};

  if (inputs.death.active) {
    sample.death_variant = inputs.death.variant;
    if (inputs.death.dying) {
      sample.is_dying = true;
      sample.death_progress =
          inputs.death.state_duration > 0.0F
              ? std::clamp(
                    inputs.death.state_time / inputs.death.state_duration, 0.0F, 1.0F)
              : 1.0F;
    } else {
      sample.is_dead = true;
      sample.death_progress = 1.0F;
    }
    return sample;
  }

  // A melee lock is an orthogonal, persistent gameplay transaction. It must
  // survive authored WindUp/Strike/Recover states; those states only decide
  // where the attack phase comes from. Previously this flag was set only when
  // the lock supplied a fallback attack, causing rooted pose layers and fight
  // indicators to switch off for the duration of every authored strike.
  sample.is_in_melee_lock = inputs.melee_lock.in_lock && inputs.melee_lock.participates;

  if (inputs.construction.active) {
    sample.is_constructing = true;
    float const elapsed = std::max(
        0.0F, inputs.construction.build_time - inputs.construction.time_remaining);
    sample.construction_progress =
        wrap_action_phase(elapsed * inputs.construction.cycles_per_second);
  }

  if (inputs.combat.has_state) {
    sample.attack_variant = inputs.combat.attack_variant;
    sample.finisher_attack = inputs.combat.finisher_attack;
    sample.attack_offset = inputs.combat.attack_offset;
    sample.has_attack_offset = true;
    sample.attack_family = inputs.combat.attack_family;

    if (inputs.combat.phase != CombatPhase::Idle) {
      sample.combat_phase = inputs.combat.phase;
      if (inputs.combat.phase_duration > 0.0F) {
        sample.combat_phase_progress =
            inputs.combat.phase_time / inputs.combat.phase_duration;
      }
      sample.is_attacking = true;
      sample.attack_from_combat_state = true;
      sample.is_melee = (sample.attack_family == CombatAttackFamily::None)
                            ? inputs.combat.fallback_mode_is_melee
                            : (sample.attack_family != CombatAttackFamily::Bow);
    }
  }

  if (!sample.is_attacking && sample.is_in_melee_lock) {
    sample.is_attacking = true;
    sample.is_melee = true;
    sample.attack_from_melee_lock = true;
    if (sample.attack_family == CombatAttackFamily::None) {
      sample.attack_family = inputs.melee_lock.fallback_attack_family;
    }
  }

  if (inputs.hit_reaction.active) {
    sample.is_hit_reacting = true;
    float const progress =
        inputs.hit_reaction.reaction_duration > 0.0F
            ? inputs.hit_reaction.reaction_time / inputs.hit_reaction.reaction_duration
            : 1.0F;
    float const envelope = std::max(0.0F, 1.0F - progress);
    sample.hit_reaction_intensity = inputs.hit_reaction.intensity * envelope;
    float const recoil_envelope = envelope * envelope;
    sample.hit_recoil_x = inputs.hit_reaction.knockback_x * recoil_envelope;
    sample.hit_recoil_z = inputs.hit_reaction.knockback_z * recoil_envelope;
  }

  if (sample.is_attacking && !sample.is_melee && !sample.is_in_melee_lock &&
      inputs.cast.has_projectile_cast) {
    sample.is_casting = true;
    sample.cast_kind = inputs.cast.projectile_is_fireball ? CastVisualKind::Fireball
                                                          : CastVisualKind::None;
  }

  return sample;
}

auto resolve_humanoid_commander_jump_pose(
    const HumanoidCommanderJumpPoseInputs& inputs) noexcept
    -> HumanoidCommanderJumpPose {
  if (!inputs.has_commander) {
    return {};
  }
  return {
      .active = inputs.active,
      .phase = std::clamp(inputs.phase, 0.0F, 1.0F),
      .height_offset = std::max(0.0F, inputs.height_offset),
  };
}

auto resolve_humanoid_commander_flag_rally_pose(
    const HumanoidCommanderFlagRallyPoseInputs& inputs) noexcept
    -> HumanoidCommanderFlagRallyPose {
  if (!inputs.has_commander || !inputs.planting) {
    return {};
  }
  if (inputs.cost <= 0.0F) {
    return {.active = true, .phase = 1.0F};
  }
  return {
      .active = true,
      .phase = std::clamp(1.0F - (inputs.animation_timer / inputs.cost), 0.0F, 1.0F),
  };
}

auto approximate_humanoid_attack_phase(const HumanoidAttackPhaseInputs& inputs) noexcept
    -> float {
  if (!inputs.is_attacking) {
    return 0.0F;
  }
  if (inputs.combat_phase == CombatPhase::Idle) {
    return wrap_action_phase(inputs.sample_time +
                             (inputs.has_attack_offset ? inputs.attack_offset : 0.0F));
  }

  auto const window =
      attack_phase_window(inputs.combat_phase, false, inputs.finisher_attack);
  return window.start + (window.end - window.start) *
                            std::clamp(inputs.combat_phase_progress, 0.0F, 1.0F);
}

} // namespace Animation
