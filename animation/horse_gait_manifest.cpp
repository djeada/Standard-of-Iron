#include "horse_gait_manifest.h"

#include <algorithm>

namespace Animation {

auto resolve_horse_desired_gait(const HorseGaitSelectionInputs& inputs) noexcept
    -> HorseGaitType {
  if (!inputs.has_locomotion_input) {
    return HorseGaitType::Idle;
  }

  constexpr float k_idle_speed_max = 0.5F;
  constexpr float k_walk_speed_max = 3.0F;
  constexpr float k_trot_speed_max = 5.5F;
  constexpr float k_canter_speed_max = 8.0F;
  constexpr float k_gait_hysteresis = 0.35F;

  float const idle_up = k_idle_speed_max + k_gait_hysteresis;
  float const idle_dn = k_idle_speed_max - k_gait_hysteresis;
  float const walk_up = k_walk_speed_max + k_gait_hysteresis;
  float const walk_dn = k_walk_speed_max - k_gait_hysteresis;
  float const trot_up = k_trot_speed_max + k_gait_hysteresis;
  float const trot_dn = k_trot_speed_max - k_gait_hysteresis;
  float const canter_up = k_canter_speed_max + k_gait_hysteresis;
  float const canter_dn = k_canter_speed_max - k_gait_hysteresis;
  auto const upshift = [&](float threshold) {
    return inputs.speed >= threshold;
  };
  auto const downshift = [&](float threshold) {
    return inputs.speed < threshold;
  };

  switch (inputs.anchor) {
  case HorseGaitType::Idle:
    if (!inputs.anim_has_motion && inputs.speed < k_idle_speed_max) {
      return HorseGaitType::Idle;
    }
    if (upshift(idle_up)) {
      if (upshift(walk_up)) {
        if (upshift(trot_up)) {
          return upshift(canter_up) ? HorseGaitType::Gallop : HorseGaitType::Canter;
        }
        return HorseGaitType::Trot;
      }
      return HorseGaitType::Walk;
    }
    return inputs.anim_has_motion ? HorseGaitType::Walk : HorseGaitType::Idle;
  case HorseGaitType::Walk:
    if (downshift(idle_dn) && !inputs.anim_has_motion) {
      return HorseGaitType::Idle;
    }
    if (upshift(walk_up)) {
      return HorseGaitType::Trot;
    }
    return HorseGaitType::Walk;
  case HorseGaitType::Trot:
    if (downshift(walk_dn)) {
      return HorseGaitType::Walk;
    }
    if (upshift(trot_up)) {
      return HorseGaitType::Canter;
    }
    return HorseGaitType::Trot;
  case HorseGaitType::Canter:
    if (downshift(trot_dn)) {
      return HorseGaitType::Trot;
    }
    if (upshift(canter_up)) {
      return HorseGaitType::Gallop;
    }
    return HorseGaitType::Canter;
  case HorseGaitType::Gallop:
    if (downshift(canter_dn)) {
      return HorseGaitType::Canter;
    }
    return HorseGaitType::Gallop;
  }
  return HorseGaitType::Idle;
}

auto resolve_horse_gait_transition(const HorseGaitTransitionInputs& inputs) noexcept
    -> HorseGaitTransitionSample {
  HorseGaitTransitionSample sample{
      .current = inputs.current,
      .target = inputs.target,
      .transition_progress = inputs.transition_progress,
      .transition_start_time = inputs.transition_start_time,
      .idle_bob_intensity = inputs.desired == HorseGaitType::Idle
                                ? std::clamp(inputs.idle_bob_intensity, 0.0F, 1.5F)
                                : 1.0F,
  };

  if (sample.target != inputs.desired) {
    sample.target = inputs.desired;
    sample.transition_progress = 0.0F;
    sample.transition_start_time = inputs.sample_time;
  }

  if (sample.transition_progress < 1.0F) {
    float const duration = std::max(inputs.transition_duration, 1.0e-4F);
    float const elapsed = inputs.sample_time - sample.transition_start_time;
    sample.transition_progress = std::clamp(elapsed / duration, 0.0F, 1.0F);
    if (sample.transition_progress >= 1.0F) {
      sample.current = sample.target;
    }
  } else {
    sample.current = sample.target;
  }

  return sample;
}

auto horse_playback_gait_for_transition(HorseGaitType current,
                                        HorseGaitType target,
                                        bool is_moving) noexcept -> HorseGaitType {
  if (!is_moving) {
    return HorseGaitType::Idle;
  }
  if (current == HorseGaitType::Idle && target != HorseGaitType::Idle) {
    return target;
  }
  if (target == HorseGaitType::Idle && current != HorseGaitType::Idle) {
    return current;
  }
  return current;
}

auto horse_bob_scale_for_gait(HorseGaitType gait) noexcept -> float {
  switch (gait) {
  case HorseGaitType::Idle:
    return 0.30F;
  case HorseGaitType::Walk:
    return 0.50F;
  case HorseGaitType::Trot:
    return 0.85F;
  case HorseGaitType::Canter:
    return 1.00F;
  case HorseGaitType::Gallop:
    return 1.12F;
  }
  return 0.30F;
}

} // namespace Animation
