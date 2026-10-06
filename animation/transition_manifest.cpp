#include "transition_manifest.h"

#include <algorithm>
#include <cmath>

namespace Animation {

namespace {

constexpr float k_hit_reaction_seconds = 0.12F;
constexpr float k_death_seconds = 0.12F;
constexpr float k_attack_entry_seconds = 0.12F;
constexpr float k_attack_chain_seconds = 0.10F;
constexpr float k_stance_entry_seconds = 0.20F;
constexpr float k_work_entry_seconds = 0.24F;
constexpr float k_riding_seconds = 0.18F;
constexpr float k_locomotion_seconds = 0.18F;
constexpr float k_attack_recovery_seconds = 0.20F;
constexpr float k_stance_exit_seconds = 0.22F;
constexpr float k_work_exit_seconds = 0.22F;
constexpr float k_hit_recovery_seconds = 0.16F;
constexpr float k_hit_reaction_footing_seconds = 0.24F;
constexpr float k_attack_entry_footing_seconds = 0.18F;

constexpr float k_snap_floor = 0.15F;
constexpr float k_smooth_source_tau = 0.12F;
constexpr float k_smooth_source_margin = 1.25F;

auto wrapped_phase_distance(float a, float b) noexcept -> float {
  float delta = std::fmod(std::abs(a - b), 1.0F);
  return std::min(delta, 1.0F - delta);
}

} // namespace

auto transition_family(StateId state,
                       bool hit_reaction,
                       bool dying,
                       bool working) noexcept -> TransitionFamily {
  if (dying || state == StateId::Die || state == StateId::Dead) {
    return TransitionFamily::Death;
  }
  if (hit_reaction) {
    return TransitionFamily::HitReaction;
  }
  if (working) {
    return TransitionFamily::Work;
  }
  switch (state) {
  case StateId::Hold:
    return TransitionFamily::Stance;
  case StateId::AttackMelee:
  case StateId::AttackRanged:
  case StateId::AttackSword:
  case StateId::AttackSpear:
  case StateId::AttackBow:
  case StateId::Cast:
  case StateId::RpgSwordSlashLeft:
  case StateId::RpgSwordSlashRight:
  case StateId::RpgSwordOverhead:
  case StateId::RpgSwordThrust:
  case StateId::RpgSwordFinisher:
    return TransitionFamily::Attack;
  case StateId::RidingIdle:
  case StateId::RidingCharge:
  case StateId::RidingReining:
  case StateId::RidingBowShot:
    return TransitionFamily::Riding;
  default:
    return TransitionFamily::Locomotion;
  }
}

auto humanoid_transition_timing(const HumanoidTransitionInputs& inputs) noexcept
    -> HumanoidTransitionTiming {
  auto const from = transition_family(inputs.from_state, false, false, false);
  auto const to = transition_family(
      inputs.to_state, inputs.to_hit_reaction, inputs.to_dying, inputs.to_working);

  float upper = k_locomotion_seconds;
  float lower = -1.0F;
  switch (to) {
  case TransitionFamily::HitReaction:
    upper = k_hit_reaction_seconds;
    lower = k_hit_reaction_footing_seconds;
    break;
  case TransitionFamily::Death:
    upper = k_death_seconds;
    break;
  case TransitionFamily::Attack:
    upper = from == TransitionFamily::Attack ? k_attack_chain_seconds
                                             : k_attack_entry_seconds;
    lower = from == TransitionFamily::Attack ? -1.0F : k_attack_entry_footing_seconds;
    break;
  case TransitionFamily::Stance:
    upper = k_stance_entry_seconds;
    break;
  case TransitionFamily::Work:
    upper = k_work_entry_seconds;
    break;
  case TransitionFamily::Riding:
    upper = k_riding_seconds;
    break;
  case TransitionFamily::Locomotion:
    switch (from) {
    case TransitionFamily::Attack:
      upper = k_attack_recovery_seconds;
      break;
    case TransitionFamily::Stance:
      upper = k_stance_exit_seconds;
      break;
    case TransitionFamily::Work:
      upper = k_work_exit_seconds;
      break;
    case TransitionFamily::HitReaction:
      upper = k_hit_recovery_seconds;
      break;
    default:
      upper = k_locomotion_seconds;
      break;
    }
    break;
  }
  auto const bounded = [](float seconds) {
    return std::clamp(
        seconds, k_humanoid_transition_min_seconds, k_humanoid_transition_max_seconds);
  };
  return {.upper_body_seconds = bounded(upper),
          .lower_body_seconds = bounded(lower > 0.0F ? lower : upper)};
}

auto same_clip_instance(const ClipMixEntry& before,
                        const ClipMixEntry& after) noexcept -> bool {
  if (before.clip != after.clip ||
      wrapped_phase_distance(before.phase, after.phase) > k_transition_phase_break) {
    return false;
  }
  if (after.loops) {
    return true;
  }
  float const rewind = before.phase - after.phase;
  bool const wrapped_to_start = rewind > 1.0F - k_transition_phase_break;
  return rewind <= k_transition_phase_rewind || wrapped_to_start;
}

auto clip_mix_discontinuity(const ClipMix& before,
                            const ClipMix& after) noexcept -> float {
  float total = 0.0F;
  std::array<bool, 3> before_matched{false, false, false};
  for (auto const& now : after) {
    if (now.clip == k_unmapped_clip || now.weight <= 0.0F) {
      continue;
    }
    float matched_weight = 0.0F;
    for (std::size_t index = 0; index < before.size(); ++index) {
      auto const& then = before[index];
      if (before_matched[index] || then.weight <= 0.0F ||
          !same_clip_instance(then, now)) {
        continue;
      }
      before_matched[index] = true;
      matched_weight = then.weight;
      break;
    }
    total += std::abs(now.weight - matched_weight);
  }
  for (std::size_t index = 0; index < before.size(); ++index) {
    if (!before_matched[index] && before[index].clip != k_unmapped_clip) {
      total += std::max(0.0F, before[index].weight);
    }
  }
  return 0.5F * total;
}

auto humanoid_transition_snap_threshold(float delta_seconds) noexcept -> float {
  float const smooth_step =
      k_smooth_source_margin * std::max(0.0F, delta_seconds) / k_smooth_source_tau;
  return std::max(k_snap_floor, smooth_step);
}

auto humanoid_transition_link_weight(float elapsed_seconds,
                                     float duration_seconds) noexcept -> float {
  if (duration_seconds <= 0.0F) {
    return 0.0F;
  }
  float const remaining =
      std::clamp(1.0F - elapsed_seconds / duration_seconds, 0.0F, 1.0F);
  return remaining * remaining * (3.0F - 2.0F * remaining);
}

} // namespace Animation
