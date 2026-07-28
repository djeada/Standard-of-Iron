#include "ambient_pose_manifest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>

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

// Which ambient idles a unit category is allowed to play. Squatting is infantry
// only; a mounted rider obviously cannot sit down on the ground.
[[nodiscard]] auto
ambient_catalog(bool mounted) noexcept -> std::span<const HumanoidAmbientIdle> {
  static constexpr std::array<HumanoidAmbientIdle, 4> k_infantry{
      HumanoidAmbientIdle::SitDown,
      HumanoidAmbientIdle::Jump,
      HumanoidAmbientIdle::RaiseWeapon,
      HumanoidAmbientIdle::ShiftWeight};
  static constexpr std::array<HumanoidAmbientIdle, 2> k_mounted{
      HumanoidAmbientIdle::RaiseWeapon, HumanoidAmbientIdle::ShiftWeight};
  return mounted ? std::span<const HumanoidAmbientIdle>{k_mounted}
                 : std::span<const HumanoidAmbientIdle>{k_infantry};
}

[[nodiscard]] auto
pick_ambient_type(std::uint32_t seed,
                  std::uint32_t play_count,
                  bool mounted,
                  HumanoidAmbientIdle previous) noexcept -> HumanoidAmbientIdle {
  auto const catalog = ambient_catalog(mounted);
  std::uint32_t const selector = hash_u32(seed ^ (play_count * 0x9E3779B9U));
  auto index = static_cast<std::size_t>(selector % catalog.size());
  if (catalog.size() > 1U && catalog[index] == previous) {
    index = (index + 1U + ((selector >> 8U) % (catalog.size() - 1U))) % catalog.size();
  }
  return catalog[index];
}

[[nodiscard]] auto roll_cooldown(std::uint32_t seed,
                                 std::uint32_t play_count) noexcept -> float {
  return k_ambient_cooldown_min +
         hash_to_unit(seed ^ (play_count * 0x85EBCA6BU) ^ 0x1D3C9F5BU) *
             k_ambient_cooldown_range;
}

[[nodiscard]] auto advance_ambient_state(
    const HumanoidAmbientTickInputs& inputs) noexcept -> HumanoidAmbientRuntimeState {
  HumanoidAmbientRuntimeState state = inputs.previous;
  float const dt = state.initialized
                       ? std::clamp(inputs.sample_time - state.last_sample_time,
                                    0.0F,
                                    k_ambient_max_step)
                       : 0.0F;
  if (!state.initialized) {
    // Without this every soldier in a formation clears the minimum idle time on
    // the same frame and the whole unit squats in unison. Spreading the very
    // first cooldown staggers the opening wave the same way later cooldowns
    // stagger every wave after it.
    state.cooldown = hash_to_unit(inputs.seed ^ 0x7A3B19C5U) * k_ambient_initial_spread;
    state.initialized = true;
  }
  state.last_sample_time = inputs.sample_time;

  // An ineligible unit never cuts to the base pose; it retreats through BlendOut
  // so a move order issued mid-squat still stands the soldier up.
  if (!inputs.eligible && state.stage != HumanoidAmbientStage::Dormant &&
      state.stage != HumanoidAmbientStage::BlendOut) {
    state.stage = HumanoidAmbientStage::BlendOut;
  }

  switch (state.stage) {
  case HumanoidAmbientStage::Dormant: {
    state.blend = 0.0F;
    state.clip_phase = 0.0F;
    state.type = HumanoidAmbientIdle::None;
    state.cooldown = std::max(0.0F, state.cooldown - dt);
    bool const may_start = inputs.eligible && state.cooldown <= 0.0F &&
                           inputs.idle_duration >= k_ambient_min_idle_duration;
    if (may_start) {
      state.type = pick_ambient_type(
          inputs.seed, state.play_count, inputs.mounted, state.previous_type);
      state.previous_type = state.type;
      state.stage = HumanoidAmbientStage::BlendIn;
      state.play_count += 1U;
    }
    break;
  }
  case HumanoidAmbientStage::BlendIn: {
    state.blend = std::min(1.0F, state.blend + dt / k_ambient_blend_in_duration);
    state.clip_phase = std::min(1.0F, state.clip_phase + dt / k_ambient_clip_duration);
    if (state.blend >= 1.0F) {
      state.stage = HumanoidAmbientStage::Hold;
    }
    break;
  }
  case HumanoidAmbientStage::Hold: {
    state.blend = 1.0F;
    state.clip_phase = std::min(1.0F, state.clip_phase + dt / k_ambient_clip_duration);
    // Start fading before the clip runs out so the blend finishes exactly as the
    // clip reaches its final frame; otherwise the tail is a hard stop.
    float const fade_start =
        1.0F - (k_ambient_blend_out_duration / k_ambient_clip_duration);
    if (state.clip_phase >= fade_start) {
      state.stage = HumanoidAmbientStage::BlendOut;
    }
    break;
  }
  case HumanoidAmbientStage::BlendOut: {
    state.blend = std::max(0.0F, state.blend - dt / k_ambient_blend_out_duration);
    state.clip_phase = std::min(1.0F, state.clip_phase + dt / k_ambient_clip_duration);
    if (state.blend <= 0.0F) {
      state.stage = HumanoidAmbientStage::Dormant;
      state.type = HumanoidAmbientIdle::None;
      state.clip_phase = 0.0F;
      state.cooldown = roll_cooldown(inputs.seed, state.play_count);
    }
    break;
  }
  }

  return state;
}

} // namespace

auto resolve_humanoid_ambient_tick(const HumanoidAmbientTickInputs& inputs) noexcept
    -> HumanoidAmbientTickSample {
  HumanoidAmbientTickSample result{};
  result.state = advance_ambient_state(inputs);
  if (result.state.stage == HumanoidAmbientStage::Dormant ||
      result.state.type == HumanoidAmbientIdle::None) {
    return result;
  }
  result.sample = {
      .active = true,
      .type = result.state.type,
      .phase = std::clamp(result.state.clip_phase, 0.0F, 1.0F),
      .blend = std::clamp(result.state.blend, 0.0F, 1.0F),
  };
  return result;
}

auto humanoid_ambient_eligible(const HumanoidAmbientSelectionInputs& inputs) noexcept
    -> bool {
  // Mounted riders are excluded until saddle-aligned ambient clips are baked.
  // Their Idle resolves to `riding_idle`, and ambient variants are indexed off the
  // standing `idle` clip, so there is nothing correct for a rider to play yet — the
  // riding idle loop already breathes on its own. See ambient_catalog().
  return !inputs.mounted && !inputs.has_locomotion && !inputs.attacking &&
         !inputs.in_hold_mode && !inputs.guarding && !inputs.exiting_guard &&
         !inputs.constructing && !inputs.healing && !inputs.hit_reacting &&
         !inputs.dying && !inputs.dead && !inputs.routing;
}

auto resolve_humanoid_ambient_selection(
    const HumanoidAmbientSelectionInputs& inputs) noexcept
    -> HumanoidAmbientTickSample {
  HumanoidAmbientTickSample scripted{};
  scripted.state = inputs.ambient_state;

  // Scripted one-shots outrank the ambient scheduler and are fully weighted:
  // they are an explicit order, not idle flavour.
  if (inputs.jump_active) {
    scripted.sample = {
        .active = true,
        .type = HumanoidAmbientIdle::Jump,
        .phase = std::clamp(inputs.jump_phase, 0.0F, 1.0F),
        .blend = 1.0F,
    };
    return scripted;
  }

  if (inputs.flag_rally_active) {
    scripted.sample = {
        .active = true,
        .type = HumanoidAmbientIdle::PlantFlag,
        .phase = std::clamp(inputs.flag_rally_phase, 0.0F, 1.0F),
        .blend = 1.0F,
    };
    return scripted;
  }

  return resolve_humanoid_ambient_tick({
      .previous = inputs.ambient_state,
      .sample_time = inputs.sample_time,
      .eligible = humanoid_ambient_eligible(inputs),
      .mounted = inputs.mounted,
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
