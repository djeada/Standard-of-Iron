#include "ambient_pose_manifest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace Animation {

namespace {

[[nodiscard]] auto smooth01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto hash_to_unit(std::uint32_t x) noexcept -> float {
  x ^= x >> 16U;
  x *= 0x7feb352dU;
  x ^= x >> 15U;
  x *= 0x846ca68bU;
  x ^= x >> 16U;
  return static_cast<float>(x & 0x7FFFFFU) / static_cast<float>(0x7FFFFFU);
}

[[nodiscard]] auto hash_u32(std::uint32_t x) noexcept -> std::uint32_t {
  x ^= x >> 16U;
  x *= 0x7feb352dU;
  x ^= x >> 15U;
  x *= 0x846ca68bU;
  x ^= x >> 16U;
  return x;
}

[[nodiscard]] auto
plateau01(float phase, float enter_end, float exit_start) noexcept -> float {
  if (phase < enter_end) {
    return smooth01(phase / enter_end);
  }
  if (phase < exit_start) {
    return 1.0F;
  }
  return smooth01((1.0F - phase) / (1.0F - exit_start));
}

} // namespace

auto resolve_humanoid_ambient_schedule(
    const HumanoidAmbientScheduleInputs& inputs) noexcept
    -> HumanoidAmbientScheduleSample {
  constexpr float k_min_idle_duration = 5.0F;
  constexpr float k_wave_period = 18.0F;
  constexpr float k_activation_stagger_max = 1.75F;
  constexpr float k_activation_duration_base = 3.4F;
  constexpr float k_activation_duration_range = 1.3F;
  constexpr std::uint32_t k_participation_bucket_count = 15U;
  constexpr std::uint32_t k_participation_bucket_min = 3U;
  constexpr std::uint32_t k_participation_bucket_range = 3U;

  if (inputs.idle_duration < k_min_idle_duration) {
    return {};
  }

  float const ambient_elapsed = inputs.idle_duration - k_min_idle_duration;
  auto const cycle_number = static_cast<std::uint32_t>(ambient_elapsed / k_wave_period);
  float const cycle_time =
      ambient_elapsed - static_cast<float>(cycle_number) * k_wave_period;
  std::uint32_t const cycle_mix = hash_u32(cycle_number ^ 0x9E3779B9U);
  float const participation_ratio =
      static_cast<float>(k_participation_bucket_min +
                         (cycle_mix % k_participation_bucket_range)) /
      static_cast<float>(k_participation_bucket_count);
  if (hash_to_unit(inputs.seed ^ cycle_mix ^ 0x6C8E9CF5U) >= participation_ratio) {
    return {};
  }

  float const activation_start =
      hash_to_unit(inputs.seed ^ cycle_mix ^ 0x41D64E6DU) * k_activation_stagger_max;
  float const activation_duration =
      k_activation_duration_base +
      hash_to_unit(inputs.seed ^ cycle_mix ^ 0xA511E9B3U) * k_activation_duration_range;
  float const activation_time = cycle_time - activation_start;
  if (activation_time < 0.0F || activation_time >= activation_duration) {
    return {};
  }

  std::uint32_t const anim_selector = hash_u32(inputs.seed ^ cycle_mix ^ 0xB5297A4DU);
  constexpr std::array<HumanoidAmbientIdle, 4> k_baked_types{
      HumanoidAmbientIdle::SitDown,
      HumanoidAmbientIdle::Jump,
      HumanoidAmbientIdle::RaiseWeapon,
      HumanoidAmbientIdle::ShiftWeight};
  auto type_index = static_cast<std::size_t>(anim_selector % k_baked_types.size());
  if (cycle_number > 0U) {
    std::uint32_t const previous_cycle_mix =
        hash_u32((cycle_number - 1U) ^ 0x9E3779B9U);
    std::uint32_t const previous_selector =
        hash_u32(inputs.seed ^ previous_cycle_mix ^ 0xB5297A4DU);
    auto const previous_index =
        static_cast<std::size_t>(previous_selector % k_baked_types.size());
    if (type_index == previous_index) {
      type_index =
          (type_index + 1U + ((anim_selector >> 4U) % (k_baked_types.size() - 1U))) %
          k_baked_types.size();
    }
  }

  return {
      .active = true,
      .type = k_baked_types[type_index],
      .phase = std::clamp(activation_time / activation_duration, 0.0F, 1.0F),
  };
}

auto resolve_humanoid_ambient_selection(
    const HumanoidAmbientSelectionInputs& inputs) noexcept
    -> HumanoidAmbientScheduleSample {
  if (inputs.jump_active) {
    return {
        .active = true,
        .type = HumanoidAmbientIdle::Jump,
        .phase = std::clamp(inputs.jump_phase, 0.0F, 1.0F),
    };
  }

  if (inputs.flag_rally_active) {
    return {
        .active = true,
        .type = HumanoidAmbientIdle::PlantFlag,
        .phase = std::clamp(inputs.flag_rally_phase, 0.0F, 1.0F),
    };
  }

  bool const eligible = !inputs.mounted && !inputs.has_locomotion &&
                        !inputs.attacking && !inputs.in_hold_mode && !inputs.guarding &&
                        !inputs.exiting_guard && !inputs.constructing &&
                        !inputs.healing && !inputs.hit_reacting && !inputs.dying &&
                        !inputs.dead;
  if (!eligible) {
    return {};
  }

  return resolve_humanoid_ambient_schedule({
      .seed = inputs.seed,
      .idle_duration = inputs.idle_duration,
  });
}

auto resolve_humanoid_ambient_pose(const HumanoidAmbientPoseInputs& inputs) noexcept
    -> HumanoidAmbientPoseSample {
  float const phase = std::clamp(inputs.phase, 0.0F, 1.0F);
  if (inputs.type == HumanoidAmbientIdle::None) {
    return {};
  }

  HumanoidAmbientPoseSample sample{};
  float const intensity = std::sin(phase * std::numbers::pi_v<float>);

  switch (inputs.type) {
  case HumanoidAmbientIdle::SitDown: {
    float const sit_intensity = plateau01(phase, 0.30F, 0.74F);
    float const sit_drop = sit_intensity * 0.18F;
    sample.pelvis_y_delta -= sit_drop;
    sample.shoulder_l_y_delta -= sit_drop * 0.70F;
    sample.shoulder_r_y_delta -= sit_drop * 0.70F;
    sample.neck_y_delta -= sit_drop * 0.62F;
    sample.head_y_delta -= sit_drop * 0.55F;
    sample.head_z_delta += sit_intensity * 0.025F;
    sample.knee_l_y_delta -= sit_drop * 0.30F;
    sample.knee_r_y_delta -= sit_drop * 0.30F;
    sample.knee_l_z_delta += sit_intensity * 0.09F;
    sample.knee_r_z_delta += sit_intensity * 0.09F;
    sample.foot_l_x_delta -= sit_intensity * 0.025F;
    sample.foot_r_x_delta += sit_intensity * 0.025F;
    break;
  }
  case HumanoidAmbientIdle::ShuffleFeet: {
    float const shuffle_phase = phase * 2.0F * std::numbers::pi_v<float>;
    float const shuffle_amount = std::sin(shuffle_phase) * intensity * 0.04F;
    sample.foot_l_z_delta += shuffle_amount;
    sample.foot_r_z_delta -= shuffle_amount;
    sample.knee_l_z_delta += shuffle_amount * 0.5F;
    sample.knee_r_z_delta -= shuffle_amount * 0.5F;
    break;
  }
  case HumanoidAmbientIdle::TapFoot: {
    constexpr float k_tap_frequency_multiplier = 6.0F;
    float const tap_phase = std::fmod(phase * k_tap_frequency_multiplier, 1.0F);
    float const tap_lift = (tap_phase < 0.3F)
                               ? std::sin(tap_phase / 0.3F * std::numbers::pi_v<float>)
                               : 0.0F;
    float const tap_amount = tap_lift * intensity * 0.03F;
    sample.foot_r_y_delta += tap_amount;
    sample.knee_r_y_delta += tap_amount * 0.3F;
    break;
  }
  case HumanoidAmbientIdle::ShiftWeight: {
    float const shift_hold = plateau01(phase, 0.32F, 0.78F);
    float const settle = std::sin(phase * 2.0F * std::numbers::pi_v<float>) * 0.012F;
    float const shift_amount = shift_hold * 0.075F + settle * intensity;
    sample.pelvis_x_delta += shift_amount;
    sample.shoulder_l_x_delta += shift_amount * 0.45F;
    sample.shoulder_r_x_delta += shift_amount * 0.45F;
    sample.neck_x_delta += shift_amount * 0.30F;
    sample.head_x_delta += shift_amount * 0.22F;
    sample.head_z_delta -= shift_hold * 0.018F;
    sample.knee_l_y_delta -= shift_hold * 0.025F;
    sample.knee_r_y_delta += shift_hold * 0.012F;
    break;
  }
  case HumanoidAmbientIdle::StepInPlace: {
    float step_phase = phase * 2.0F;
    bool const is_left_step = step_phase < 1.0F;
    if (!is_left_step) {
      step_phase -= 1.0F;
    }
    float const step_lift =
        std::sin(step_phase * std::numbers::pi_v<float>) * intensity * 0.05F;
    if (is_left_step) {
      sample.foot_l_y_delta += step_lift;
      sample.knee_l_y_delta += step_lift * 0.6F;
    } else {
      sample.foot_r_y_delta += step_lift;
      sample.knee_r_y_delta += step_lift * 0.6F;
    }
    break;
  }
  case HumanoidAmbientIdle::BendKnee: {
    float const bend_amount = intensity * 0.06F;
    sample.knee_l_y_delta -= bend_amount;
    sample.knee_l_z_delta += bend_amount * 0.4F;
    sample.foot_l_y_delta += bend_amount * 0.2F;
    float const shift = bend_amount * 0.25F;
    sample.pelvis_x_delta += shift;
    sample.shoulder_l_x_delta += shift;
    sample.shoulder_r_x_delta += shift;
    sample.neck_x_delta += shift;
    sample.head_x_delta += shift;
    break;
  }
  case HumanoidAmbientIdle::RaiseWeapon: {
    float const raise_intensity = plateau01(phase, 0.34F, 0.78F);
    float const wave =
        std::sin(phase * 4.0F * std::numbers::pi_v<float>) * raise_intensity;
    float const right_raise = raise_intensity * 0.28F;
    float const left_raise = raise_intensity * 0.07F;
    sample.hand_r_y_delta += right_raise;
    sample.elbow_r_y_delta += right_raise * 0.55F;
    sample.hand_r_x_delta += wave * 0.035F;
    sample.elbow_r_x_delta += wave * 0.020F;
    sample.hand_r_z_delta -= raise_intensity * 0.06F;
    sample.hand_l_y_delta += left_raise;
    sample.elbow_l_y_delta += left_raise * 0.45F;
    sample.head_y_delta += raise_intensity * 0.018F;
    sample.head_z_delta -= raise_intensity * 0.035F;
    break;
  }
  case HumanoidAmbientIdle::Jump: {
    float const prep_crouch =
        (phase < 0.34F) ? std::sin((phase / 0.34F) * std::numbers::pi_v<float>) : 0.0F;
    float const landing_crouch =
        (phase > 0.58F && phase < 0.90F)
            ? std::sin(((phase - 0.58F) / 0.32F) * std::numbers::pi_v<float>)
            : 0.0F;
    float const air =
        (phase > 0.26F && phase < 0.66F)
            ? std::sin(((phase - 0.26F) / 0.40F) * std::numbers::pi_v<float>)
            : 0.0F;
    float const airborne_scale = inputs.airborne ? 1.0F : 0.45F;
    float const crouch_amount = prep_crouch * 0.75F + landing_crouch;
    float const torso_lift = air * (0.020F + 0.140F * airborne_scale) -
                             crouch_amount * (0.020F + 0.075F * airborne_scale);
    float const knee_lift =
        air * (0.010F + 0.080F * airborne_scale) - crouch_amount * 0.012F;
    float const foot_lift = air * (0.010F + 0.110F * airborne_scale);
    float const knee_drive = crouch_amount * 0.080F + air * 0.090F * airborne_scale;
    float const foot_drive = crouch_amount * 0.020F + air * 0.035F * airborne_scale;
    float const hand_lift = air * 0.050F * airborne_scale - crouch_amount * 0.030F;
    sample.pelvis_y_delta += torso_lift;
    sample.shoulder_l_y_delta += torso_lift * 0.92F;
    sample.shoulder_r_y_delta += torso_lift * 0.92F;
    sample.neck_y_delta += torso_lift * 0.96F;
    sample.head_y_delta += torso_lift;
    sample.knee_l_y_delta += torso_lift * 0.55F + knee_lift;
    sample.knee_r_y_delta += torso_lift * 0.55F + knee_lift;
    sample.knee_l_z_delta += knee_drive;
    sample.knee_r_z_delta += knee_drive;
    sample.foot_l_y_delta += foot_lift;
    sample.foot_r_y_delta += foot_lift;
    sample.foot_l_z_delta += foot_drive;
    sample.foot_r_z_delta += foot_drive;
    sample.hand_l_y_delta += hand_lift;
    sample.hand_r_y_delta += hand_lift;
    break;
  }
  case HumanoidAmbientIdle::PlantFlag: {
    float const prepare = plateau01(phase, 0.08F, 0.26F);
    float const plant = plateau01(phase, 0.30F, 0.72F);
    float const recover =
        (phase > 0.72F)
            ? std::sin(((phase - 0.72F) / 0.28F) * std::numbers::pi_v<float>)
            : 0.0F;
    float const lean = plant * 0.11F + recover * 0.03F;
    float const crouch = prepare * 0.05F + plant * 0.16F;
    float const right_drive = prepare * 0.16F + plant * 0.22F - recover * 0.07F;
    float const left_brace = prepare * 0.08F + plant * 0.14F - recover * 0.04F;
    sample.pelvis_y_delta -= crouch;
    sample.pelvis_z_delta += lean * 0.35F;
    sample.shoulder_l_y_delta -= crouch * 0.42F;
    sample.shoulder_r_y_delta -= crouch * 0.32F;
    sample.shoulder_l_z_delta += lean * 0.55F;
    sample.shoulder_r_z_delta += lean * 0.72F;
    sample.neck_z_delta += lean * 0.72F;
    sample.head_z_delta += lean * 0.86F;
    sample.head_y_delta -= crouch * 0.22F;
    sample.knee_l_y_delta -= crouch * 0.18F;
    sample.knee_r_y_delta -= crouch * 0.40F;
    sample.knee_l_z_delta += plant * 0.05F;
    sample.knee_r_z_delta += plant * 0.15F;
    sample.foot_l_x_delta -= plant * 0.03F;
    sample.foot_l_z_delta -= plant * 0.04F;
    sample.foot_r_x_delta += plant * 0.02F;
    sample.foot_r_z_delta += plant * 0.12F;
    sample.hand_r_y_delta -= right_drive;
    sample.hand_r_z_delta += plant * 0.16F + prepare * 0.04F;
    sample.hand_r_x_delta += prepare * 0.03F;
    sample.elbow_r_y_delta -= right_drive * 0.50F;
    sample.elbow_r_z_delta += plant * 0.11F;
    sample.elbow_r_x_delta += prepare * 0.02F;
    sample.hand_l_y_delta += -left_brace * 0.75F + recover * 0.02F;
    sample.hand_l_z_delta += plant * 0.10F;
    sample.hand_l_x_delta -= plant * 0.04F;
    sample.elbow_l_y_delta -= left_brace * 0.35F;
    sample.elbow_l_z_delta += plant * 0.07F;
    sample.elbow_l_x_delta -= plant * 0.03F;
    break;
  }
  case HumanoidAmbientIdle::None:
    break;
  }

  return sample;
}

} // namespace Animation
