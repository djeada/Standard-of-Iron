#include "locomotion_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace Animation {

namespace {

constexpr float k_abs_sine_mean = 0.63662F;
constexpr float k_torso_lag_phase = 0.06F;
constexpr float k_shoulder_bob_lag_phase = 0.045F;
constexpr float k_neck_bob_lag_phase = 0.065F;
constexpr float k_head_bob_lag_phase = 0.085F;
constexpr float k_arm_swing_phase_shift = 0.30F;
constexpr float k_arm_forward_bias = 0.86F;
constexpr float k_arm_backward_bias = 1.18F;

constexpr float k_reverse_stride_scale = 0.74F;

constexpr float k_foot_toe_length = 0.165F;
constexpr float k_foot_heel_length = 0.060F;

constexpr float k_walk_planted_fraction = 0.60F;
constexpr float k_walk_stance_travel = 0.95F;
constexpr float k_walk_cycle_distance = k_walk_stance_travel / k_walk_planted_fraction;
static_assert(k_walk_cycle_distance > k_humanoid_walk_cycle_distance - 0.001F &&
              k_walk_cycle_distance < k_humanoid_walk_cycle_distance + 0.001F);

constexpr float k_run_planted_fraction = 0.32F;
constexpr float k_run_stance_travel = 0.96F;
constexpr float k_run_cycle_distance = k_run_stance_travel / k_run_planted_fraction;
static_assert(k_run_cycle_distance > k_humanoid_run_cycle_distance - 0.001F &&
              k_run_cycle_distance < k_humanoid_run_cycle_distance + 0.001F);

constexpr float k_min_cycle_time = 0.52F;
constexpr float k_max_cycle_time = 1.30F;
constexpr float k_min_cadence_speed = 0.35F;

constexpr float k_run_bob_phase_advance = 0.07F;

constexpr float k_run_flight_lift = 0.030F;
constexpr float k_run_locomotion_blend_floor = 0.92F;
constexpr float k_run_blend_base = 0.80F;

[[nodiscard]] auto ankle_lift_for_pitch(float pitch) noexcept -> float {
  return (pitch < 0.0F) ? (k_foot_toe_length * -std::sin(pitch))
                        : (k_foot_heel_length * std::sin(pitch));
}

[[nodiscard]] auto smoothing_alpha(float dt, float tau) noexcept -> float {
  if (dt <= 1.0e-4F) {
    return 1.0F;
  }
  return 1.0F - std::exp(-dt / std::max(tau, 1.0e-4F));
}

[[nodiscard]] auto
smooth_towards(float current, float target, float dt, float tau) noexcept -> float {
  return current + (target - current) * smoothing_alpha(dt, tau);
}

[[nodiscard]] auto lerp(float a, float b, float t) noexcept -> float {
  return a + (b - a) * t;
}

[[nodiscard]] auto hash_01(std::uint32_t x) noexcept -> float {
  x ^= x << 13U;
  x ^= x >> 17U;
  x ^= x << 5U;
  return static_cast<float>(x & 0xFFFFFFU) / static_cast<float>(0x01000000U);
}

[[nodiscard]] auto normalized_or(float x,
                                 float z,
                                 float fallback_x,
                                 float fallback_z) noexcept -> std::pair<float, float> {
  float out_x = x;
  float out_z = z;
  float len_sq = out_x * out_x + out_z * out_z;
  if (len_sq <= 1.0e-6F) {
    out_x = fallback_x;
    out_z = fallback_z;
    len_sq = out_x * out_x + out_z * out_z;
  }
  if (len_sq <= 1.0e-6F) {
    out_x = 0.0F;
    out_z = 1.0F;
    len_sq = 1.0F;
  }
  float const inv_len = 1.0F / std::sqrt(len_sq);
  return {out_x * inv_len, out_z * inv_len};
}

[[nodiscard]] auto signed_turn_amount(float entity_forward_x,
                                      float entity_forward_z,
                                      float locomotion_direction_x,
                                      float locomotion_direction_z) noexcept -> float {
  auto const [forward_x, forward_z] =
      normalized_or(entity_forward_x, entity_forward_z, 0.0F, 1.0F);
  auto const [motion_x, motion_z] = normalized_or(
      locomotion_direction_x, locomotion_direction_z, forward_x, forward_z);
  return std::clamp((forward_z * motion_x) - (forward_x * motion_z), -1.0F, 1.0F);
}

[[nodiscard]] auto
signed_travel_alignment(float entity_forward_x,
                        float entity_forward_z,
                        float locomotion_direction_x,
                        float locomotion_direction_z) noexcept -> float {
  auto const [forward_x, forward_z] =
      normalized_or(entity_forward_x, entity_forward_z, 0.0F, 1.0F);
  auto const [motion_x, motion_z] = normalized_or(
      locomotion_direction_x, locomotion_direction_z, forward_x, forward_z);
  return std::clamp((forward_x * motion_x) + (forward_z * motion_z), -1.0F, 1.0F);
}

constexpr float k_reverse_gait_enter_alignment = -0.30F;
constexpr float k_reverse_gait_exit_alignment = -0.05F;

constexpr float k_locomotion_residual_blend = 0.02F;

struct LocomotionTargets {
  float normalized_speed{0.0F};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  float cycle_time{0.0F};
  float base_phase{0.0F};
  float turn_amount{0.0F};
  float travel_alignment{1.0F};
};

struct LocomotionPoseProfile {
  float planted_fraction{0.5F};
  float stance_forward_fraction{0.5F};
  float track_ratio{0.5F};
  float swing_lift_advance{0.0F};
  float stride_length{0.0F};
  float step_height{0.0F};
  float vertical_bob{0.0F};
  float hip_sway{0.0F};
  float shoulder_sway{0.0F};
  float shoulder_twist{0.0F};
  float pelvis_twist{0.0F};
  float forward_lean{0.0F};
  float lateral_foot_shift{0.0F};
  float arm_swing{0.0F};
  float max_arm_displacement{0.0F};
  float elbow_flex{0.0F};
  float arm_counter_shift{0.0F};
  float weight_shift{0.0F};
  float pelvis_drop{0.0F};
  float head_stabilization{0.0F};
  float contact_lift{0.0F};
  float knee_drive{0.0F};
  float heel_strike_pitch{0.0F};
  float toe_off_pitch{0.0F};
  float swing_clearance_pitch{0.0F};
};

[[nodiscard]] auto blend_profile(const LocomotionPoseProfile& a,
                                 const LocomotionPoseProfile& b,
                                 float blend) noexcept -> LocomotionPoseProfile {
  auto const mix = [blend](float from, float to) {
    return lerp(from, to, blend);
  };

  LocomotionPoseProfile out{};
  out.planted_fraction = mix(a.planted_fraction, b.planted_fraction);
  out.stance_forward_fraction =
      mix(a.stance_forward_fraction, b.stance_forward_fraction);
  out.track_ratio = mix(a.track_ratio, b.track_ratio);
  out.swing_lift_advance = mix(a.swing_lift_advance, b.swing_lift_advance);
  out.stride_length = mix(a.stride_length, b.stride_length);
  out.step_height = mix(a.step_height, b.step_height);
  out.vertical_bob = mix(a.vertical_bob, b.vertical_bob);
  out.hip_sway = mix(a.hip_sway, b.hip_sway);
  out.shoulder_sway = mix(a.shoulder_sway, b.shoulder_sway);
  out.shoulder_twist = mix(a.shoulder_twist, b.shoulder_twist);
  out.pelvis_twist = mix(a.pelvis_twist, b.pelvis_twist);
  out.forward_lean = mix(a.forward_lean, b.forward_lean);
  out.lateral_foot_shift = mix(a.lateral_foot_shift, b.lateral_foot_shift);
  out.arm_swing = mix(a.arm_swing, b.arm_swing);
  out.max_arm_displacement = mix(a.max_arm_displacement, b.max_arm_displacement);
  out.elbow_flex = mix(a.elbow_flex, b.elbow_flex);
  out.arm_counter_shift = mix(a.arm_counter_shift, b.arm_counter_shift);
  out.weight_shift = mix(a.weight_shift, b.weight_shift);
  out.pelvis_drop = mix(a.pelvis_drop, b.pelvis_drop);
  out.head_stabilization = mix(a.head_stabilization, b.head_stabilization);
  out.contact_lift = mix(a.contact_lift, b.contact_lift);
  out.knee_drive = mix(a.knee_drive, b.knee_drive);
  out.heel_strike_pitch = mix(a.heel_strike_pitch, b.heel_strike_pitch);
  out.toe_off_pitch = mix(a.toe_off_pitch, b.toe_off_pitch);
  out.swing_clearance_pitch = mix(a.swing_clearance_pitch, b.swing_clearance_pitch);
  return out;
}

[[nodiscard]] auto walk_profile(float speed_factor,
                                const HumanoidLocomotionPoseInputs& inputs) noexcept
    -> LocomotionPoseProfile {
  LocomotionPoseProfile profile{};
  profile.planted_fraction = k_walk_planted_fraction;
  profile.stance_forward_fraction = 0.46F;
  profile.track_ratio = 0.48F;
  profile.swing_lift_advance = 0.28F;
  profile.stride_length = k_walk_stance_travel * inputs.walk_speed_multiplier *
                          (0.90F + 0.10F * speed_factor);
  profile.step_height = 0.075F * (0.92F + 0.10F * speed_factor);
  profile.vertical_bob = 0.020F * speed_factor;
  profile.hip_sway = 0.024F * inputs.stance_width;
  profile.shoulder_sway = 0.013F;
  profile.shoulder_twist = 0.032F;
  profile.pelvis_twist = 0.026F;
  profile.forward_lean = 0.020F + (speed_factor - 1.0F) * 0.010F;
  profile.lateral_foot_shift = 0.008F;
  profile.arm_swing = 0.195F * inputs.arm_swing_amplitude;
  profile.max_arm_displacement = 0.215F;
  profile.elbow_flex = 0.025F;
  profile.arm_counter_shift = 0.12F;
  profile.weight_shift = 0.014F;
  profile.pelvis_drop = 0.012F;
  profile.head_stabilization = 0.74F;
  profile.contact_lift = 0.002F;
  profile.knee_drive = 0.016F;

  profile.heel_strike_pitch = 0.28F;
  profile.toe_off_pitch = -0.42F;
  profile.swing_clearance_pitch = 0.12F;
  return profile;
}

[[nodiscard]] auto run_profile(float speed_factor,
                               const HumanoidLocomotionPoseInputs& inputs) noexcept
    -> LocomotionPoseProfile {
  LocomotionPoseProfile profile{};
  profile.planted_fraction = k_run_planted_fraction;
  profile.stance_forward_fraction = 0.36F;
  profile.track_ratio = 0.30F;
  profile.swing_lift_advance = 0.90F;
  profile.stride_length = k_run_stance_travel * inputs.walk_speed_multiplier *
                          (0.90F + 0.10F * speed_factor);
  profile.step_height = 0.205F * (0.88F + 0.18F * speed_factor);
  profile.vertical_bob = 0.062F * speed_factor;
  profile.hip_sway = 0.007F * inputs.stance_width;
  profile.shoulder_sway = 0.005F;
  profile.shoulder_twist = 0.098F;
  profile.pelvis_twist = 0.078F;
  profile.forward_lean = 0.200F + (speed_factor - 1.0F) * 0.040F;
  profile.lateral_foot_shift = 0.004F;
  profile.arm_swing = 0.340F * inputs.arm_swing_amplitude;
  profile.max_arm_displacement = 0.360F;
  profile.elbow_flex = 0.460F;
  profile.arm_counter_shift = 0.16F;
  profile.weight_shift = 0.003F;
  profile.pelvis_drop = 0.040F;
  profile.head_stabilization = 0.90F;
  profile.contact_lift = 0.010F;
  profile.knee_drive = 0.070F;

  profile.heel_strike_pitch = -0.06F;
  profile.toe_off_pitch = -0.70F;
  profile.swing_clearance_pitch = 0.30F;
  return profile;
}

[[nodiscard]] auto reference_stride_distance(
    float run_blend, const HumanoidLocomotionPoseInputs& inputs) noexcept -> float {
  float const speed_scale = std::max(0.1F, inputs.walk_speed_multiplier);
  float const walk_reference = k_walk_cycle_distance * speed_scale;
  float const run_reference = k_run_cycle_distance * speed_scale;
  return lerp(walk_reference, run_reference, run_blend);
}

[[nodiscard]] auto stride_distance_scale(const HumanoidLocomotionPoseInputs& inputs,
                                         float run_blend) noexcept -> float {
  float const reference_distance = reference_stride_distance(run_blend, inputs);
  if (inputs.stride_distance > 1.0e-4F && reference_distance > 1.0e-4F) {
    return std::clamp(inputs.stride_distance / reference_distance, 0.72F, 1.28F);
  }
  return std::clamp(
      inputs.normalized_speed > 0.0F ? inputs.normalized_speed : 1.0F, 0.72F, 1.15F);
}

[[nodiscard]] auto smoothstep(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

constexpr float k_swing_tangent_ratio = 0.75F;

[[nodiscard]] auto swing_travel(float t, float planted_fraction) noexcept -> float {
  float const stance_slope = -k_swing_tangent_ratio * (1.0F - planted_fraction) /
                             std::max(0.05F, planted_fraction);
  float const t2 = t * t;
  float const t3 = t2 * t;
  return (stance_slope * ((2.0F * t3) - (3.0F * t2) + t)) + ((3.0F * t2) - (2.0F * t3));
}

[[nodiscard]] auto
smooth_pulse(float t, float start, float peak, float end) noexcept -> float {
  if (t <= start || t >= end) {
    return 0.0F;
  }
  if (t <= peak) {
    float const u =
        std::clamp((t - start) / std::max(1.0e-4F, peak - start), 0.0F, 1.0F);
    return smoothstep(u);
  }
  float const u = std::clamp((t - peak) / std::max(1.0e-4F, end - peak), 0.0F, 1.0F);
  return 1.0F - smoothstep(u);
}

[[nodiscard]] auto profile_for_pose(const HumanoidLocomotionPoseInputs& inputs,
                                    float speed_factor,
                                    float run_blend) noexcept -> LocomotionPoseProfile {
  return blend_profile(
      walk_profile(speed_factor, inputs), run_profile(speed_factor, inputs), run_blend);
}

[[nodiscard]] auto resolve_locomotion_foot(const HumanoidLocomotionPoseInputs& inputs,
                                           const LocomotionPoseProfile& profile,
                                           float phase,
                                           float lateral_sign,
                                           float planted_fraction,
                                           float stride_scale,
                                           float step_scale,
                                           float locomotion_blend,
                                           float stride_distance_scale_factor,
                                           float acceleration_push,
                                           float braking,
                                           float turn_amount,
                                           float turn_abs,
                                           float turn_step_bias,
                                           float weight_shift,
                                           PoseVec3 base_foot,
                                           float& out_pitch) noexcept -> PoseVec3 {
  float z_pos = 0.0F;
  float pitch = 0.0F;
  float foot_y = inputs.ground_y + inputs.foot_y_offset;
  float const foot_turn_scale = 1.0F - turn_amount * lateral_sign * 0.45F;
  float const acceleration_stride_scale =
      std::max(0.68F, 1.0F + acceleration_push * 0.22F - braking * 0.28F);
  float const foot_stride_length =
      profile.stride_length * stride_scale * acceleration_stride_scale *
      stride_distance_scale_factor * std::max(0.60F, foot_turn_scale);
  float const foot_step_height =
      profile.step_height * step_scale * std::max(0.70F, 1.0F - turn_abs * 0.12F) *
      std::max(0.85F, std::sqrt(stride_distance_scale_factor));
  if (phase < planted_fraction) {
    float const t = std::clamp(phase / planted_fraction, 0.0F, 1.0F);
    float const heel_settle = smooth_pulse(t, 0.0F, 0.08F, 0.24F);
    float const mid_stance = smooth_pulse(t, 0.18F, 0.50F, 0.82F);
    float const toe_off = smooth_pulse(t, 0.72F, 0.88F, 1.0F);
    z_pos =
        foot_stride_length * (profile.stance_forward_fraction - t + mid_stance * 0.03F);
    foot_y += (heel_settle * 0.55F + toe_off) * profile.contact_lift * stride_scale;
    foot_y -= mid_stance * 0.0018F * stride_scale;

    float const heel_roll = 1.0F - smoothstep(std::clamp(t / 0.22F, 0.0F, 1.0F));

    float const toe_roll = smoothstep(std::clamp((t - 0.52F) / 0.48F, 0.0F, 1.0F));
    pitch =
        (profile.heel_strike_pitch * heel_roll) + (profile.toe_off_pitch * toe_roll);
  } else {
    float const t = std::clamp((phase - planted_fraction) /
                                   std::max(1.0e-4F, 1.0F - planted_fraction),
                               0.0F,
                               1.0F);
    float const travel = swing_travel(t, planted_fraction);

    float const lift_time = t * (1.0F + profile.swing_lift_advance * (1.0F - t));
    float const lift_curve = std::sin(lift_time * std::numbers::pi_v<float>);
    float const landing_soften = smooth_pulse(t, 0.72F, 0.90F, 1.0F);
    z_pos = -foot_stride_length * (1.0F - profile.stance_forward_fraction) +
            (foot_stride_length * 1.00F) * travel;
    foot_y +=
        lift_curve * foot_step_height +
        std::sin(t * std::numbers::pi_v<float>) * profile.knee_drive * step_scale +
        landing_soften * profile.contact_lift * 0.25F;

    float const recover = smoothstep(std::clamp(t / 0.34F, 0.0F, 1.0F));
    float const reach = smoothstep(std::clamp((t - 0.45F) / 0.55F, 0.0F, 1.0F));
    pitch = std::lerp(profile.toe_off_pitch, profile.swing_clearance_pitch, recover);
    pitch = std::lerp(pitch, profile.heel_strike_pitch, reach);
  }

  out_pitch = pitch * locomotion_blend;
  foot_y += ankle_lift_for_pitch(pitch);

  float const stance_turn_bias =
      turn_amount * lateral_sign * (0.014F + 0.006F * locomotion_blend);

  float const track_x = base_foot.x * profile.track_ratio;
  float const target_x =
      track_x -
      lateral_sign * std::abs(std::sin(phase * 2.0F * std::numbers::pi_v<float>)) *
          profile.lateral_foot_shift * stride_scale +
      turn_step_bias * lateral_sign + stance_turn_bias + weight_shift * lateral_sign;
  float const target_z =
      z_pos * locomotion_blend + base_foot.z * (1.0F - locomotion_blend);

  return {
      base_foot.x + (target_x - base_foot.x) * locomotion_blend,
      inputs.ground_y + inputs.foot_y_offset +
          (foot_y - (inputs.ground_y + inputs.foot_y_offset)) * locomotion_blend,
      target_z,
  };
}

[[nodiscard]] auto
build_targets(const HumanoidLocomotionInputs& inputs) noexcept -> LocomotionTargets {
  LocomotionTargets targets{};
  bool const has_locomotion = is_moving(inputs.movement_state);
  float const reference_speed = (inputs.motion_state == HumanoidMotionState::Run)
                                    ? inputs.tuning.reference_run_speed
                                    : inputs.tuning.reference_walk_speed;
  targets.normalized_speed =
      (has_locomotion && reference_speed > 0.0001F)
          ? std::clamp(inputs.speed / reference_speed, 0.0F, 1.0F)
          : 0.0F;
  bool const running =
      has_locomotion && inputs.motion_state == HumanoidMotionState::Run;
  targets.locomotion_blend =
      has_locomotion ? std::clamp(targets.normalized_speed * 1.10F, 0.0F, 1.0F) : 0.0F;
  if (running) {
    targets.locomotion_blend =
        std::max(targets.locomotion_blend, k_run_locomotion_blend_floor);
  }
  targets.run_blend = running
                          ? std::clamp(k_run_blend_base + targets.normalized_speed *
                                                              (1.0F - k_run_blend_base),
                                       0.0F,
                                       1.0F)
                          : 0.0F;
  targets.turn_amount = has_locomotion
                            ? signed_turn_amount(inputs.entity_forward_x,
                                                 inputs.entity_forward_z,
                                                 inputs.locomotion_direction_x,
                                                 inputs.locomotion_direction_z)
                            : 0.0F;
  targets.travel_alignment =
      has_locomotion ? signed_travel_alignment(inputs.entity_forward_x,
                                               inputs.entity_forward_z,
                                               inputs.locomotion_direction_x,
                                               inputs.locomotion_direction_z)
                     : 1.0F;
  if (has_locomotion) {
    float const walk_cycle_time = humanoid_walk_cycle_time_for_speed(inputs.speed);
    float const run_cycle_time = humanoid_run_cycle_time_for_speed(inputs.speed);
    targets.cycle_time = lerp(walk_cycle_time, run_cycle_time, targets.run_blend);
  } else {
    targets.cycle_time = inputs.tuning.idle_cycle_time;
  }

  targets.cycle_time /= std::clamp(inputs.cadence_scale, 0.80F, 1.25F);
  targets.base_phase =
      wrap_locomotion_phase((inputs.sample_time + inputs.phase_offset) /
                            std::max(0.001F, targets.cycle_time));
  return targets;
}

} // namespace

auto humanoid_foot_contact_lift(float foot_pitch) noexcept -> float {
  return ankle_lift_for_pitch(foot_pitch);
}

auto wrap_locomotion_phase(float phase) noexcept -> float {
  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto humanoid_reference_cycle_time(HumanoidMotionState state,
                                   const HumanoidLocomotionTuning& tuning) noexcept
    -> float {
  if (state == HumanoidMotionState::Run) {
    return humanoid_run_cycle_time_for_speed(tuning.reference_run_speed);
  }
  if (state == HumanoidMotionState::Walk) {
    return humanoid_walk_cycle_time_for_speed(tuning.reference_walk_speed);
  }
  return tuning.idle_cycle_time;
}

auto humanoid_walk_cycle_time_for_speed(float speed) noexcept -> float {
  return std::clamp(k_walk_cycle_distance / std::max(k_min_cadence_speed, speed),
                    k_min_cycle_time,
                    k_max_cycle_time);
}

auto humanoid_run_cycle_time_for_speed(float speed) noexcept -> float {
  return std::clamp(k_run_cycle_distance / std::max(k_min_cadence_speed, speed),
                    k_min_cycle_time,
                    k_max_cycle_time);
}

auto resolve_humanoid_locomotion_sample(const HumanoidLocomotionInputs& inputs) noexcept
    -> HumanoidLocomotionSample {
  auto const targets = build_targets(inputs);

  HumanoidLocomotionPersistentState previous = inputs.previous;
  if (previous.initialized && inputs.sample_time < previous.last_sample_time &&
      inputs.allow_persistent_update) {
    previous = {};
  }

  float delta_time = 0.0F;
  if (previous.initialized) {
    delta_time = std::max(0.0F, inputs.sample_time - previous.last_sample_time);
  }

  bool const repeat_of_same_frame =
      previous.initialized && delta_time <= 0.0F && inputs.allow_persistent_update;

  HumanoidLocomotionSample sample{};
  sample.previous_seen_initialized = previous.initialized;
  sample.previous_seen_presence = previous.locomotion_presence;
  sample.state = inputs.motion_state;
  sample.speed = inputs.speed;
  sample.cycle_time = targets.cycle_time;
  sample.cycle_phase = targets.base_phase;
  sample.normalized_speed = targets.normalized_speed;
  sample.locomotion_blend = targets.locomotion_blend;
  sample.run_blend = targets.run_blend;
  sample.turn_amount = targets.turn_amount;
  sample.travel_alignment = targets.travel_alignment;
  sample.acceleration = 0.0F;

  if (previous.initialized) {
    sample.travel_alignment = smooth_towards(previous.filtered_travel_alignment,
                                             targets.travel_alignment,
                                             delta_time,
                                             inputs.tuning.turn_blend_tau);
  }

  bool const has_locomotion = is_moving(inputs.movement_state);

  sample.locomotion_presence = has_locomotion ? 1.0F : 0.0F;
  sample.run_presence =
      (has_locomotion && inputs.motion_state == HumanoidMotionState::Run) ? 1.0F : 0.0F;
  if (previous.initialized) {
    sample.locomotion_presence = smooth_towards(previous.locomotion_presence,
                                                sample.locomotion_presence,
                                                delta_time,
                                                inputs.tuning.locomotion_blend_tau);
    sample.run_presence = smooth_towards(previous.run_presence,
                                         sample.run_presence,
                                         delta_time,
                                         inputs.tuning.run_blend_tau);
  }

  bool const residual_gait = !has_locomotion && previous.initialized &&
                             sample.locomotion_presence > k_locomotion_residual_blend;
  bool const gait_running = has_locomotion || residual_gait;

  if (residual_gait && previous.state != HumanoidMotionState::Idle) {
    sample.state = previous.state;
  }

  sample.reverse_gait = previous.initialized ? previous.reverse_gait : false;
  if (has_locomotion) {
    if (sample.travel_alignment <= k_reverse_gait_enter_alignment) {
      sample.reverse_gait = true;
    } else if (sample.travel_alignment >= k_reverse_gait_exit_alignment) {
      sample.reverse_gait = false;
    }
  } else if (!residual_gait) {
    sample.reverse_gait = false;
  }

  if (previous.initialized && gait_running) {
    float const previous_cycle_time =
        (previous.cycle_time > 1.0e-4F) ? previous.cycle_time : targets.cycle_time;
    sample.cycle_time = has_locomotion ? smooth_towards(previous_cycle_time,
                                                        targets.cycle_time,
                                                        delta_time,
                                                        inputs.tuning.cadence_blend_tau)
                                       : previous_cycle_time;
    float const phase_direction = sample.reverse_gait ? -1.0F : 1.0F;
    sample.cycle_phase =
        wrap_locomotion_phase(previous.phase + phase_direction * delta_time /
                                                   std::max(0.001F, sample.cycle_time));
  }

  if (previous.initialized) {
    float const previous_filtered_speed = previous.filtered_speed;
    sample.normalized_speed = smooth_towards(previous_filtered_speed,
                                             targets.normalized_speed,
                                             delta_time,
                                             inputs.tuning.locomotion_blend_tau);
    sample.locomotion_blend = smooth_towards(previous.locomotion_blend,
                                             targets.locomotion_blend,
                                             delta_time,
                                             inputs.tuning.locomotion_blend_tau);
    sample.run_blend = smooth_towards(
        previous.run_blend, targets.run_blend, delta_time, inputs.tuning.run_blend_tau);
    sample.turn_amount = smooth_towards(previous.filtered_turn,
                                        targets.turn_amount,
                                        delta_time,
                                        inputs.tuning.turn_blend_tau);

    float instant_acceleration = 0.0F;
    if (delta_time > 1.0e-4F) {
      instant_acceleration =
          (sample.normalized_speed - previous_filtered_speed) / delta_time;
    }
    sample.acceleration = smooth_towards(previous.filtered_acceleration,
                                         std::clamp(instant_acceleration, -4.0F, 4.0F),
                                         delta_time,
                                         inputs.tuning.acceleration_blend_tau);
  }

  sample.stride_distance = gait_running ? sample.speed * sample.cycle_time *
                                              std::max(0.0F, sample.locomotion_blend)
                                        : 0.0F;

  sample.persistent = previous;
  if (inputs.has_persistent_state && inputs.allow_persistent_update &&
      !repeat_of_same_frame) {
    sample.write_persistent_state = true;
    sample.persistent.initialized = true;
    sample.persistent.last_sample_time = inputs.sample_time;
    sample.persistent.phase = sample.cycle_phase;
    sample.persistent.phase_bias = sample.cycle_phase - targets.base_phase;
    sample.persistent.cycle_time = sample.cycle_time;
    sample.persistent.filtered_speed = sample.normalized_speed;
    sample.persistent.filtered_acceleration = sample.acceleration;
    sample.persistent.filtered_turn = sample.turn_amount;
    sample.persistent.filtered_travel_alignment = sample.travel_alignment;
    sample.persistent.reverse_gait = sample.reverse_gait;
    sample.persistent.locomotion_blend = sample.locomotion_blend;
    sample.persistent.run_blend = sample.run_blend;
    sample.persistent.locomotion_presence = sample.locomotion_presence;
    sample.persistent.run_presence = sample.run_presence;
    sample.persistent.state = sample.state;
  }

  return sample;
}

auto resolve_humanoid_locomotion_action_override(
    const HumanoidLocomotionActionOverrideInputs& inputs) noexcept
    -> HumanoidLocomotionActionOverrideSample {
  if (!inputs.commander_jump_active) {
    return {};
  }
  return {
      .active = true,
      .state = HumanoidMotionState::Idle,
      .move_speed = 0.0F,
      .normalized_speed = 0.0F,
      .has_target = false,
      .airborne = true,
  };
}

auto resolve_humanoid_locomotion_phase_override(
    const HumanoidLocomotionPhaseOverrideInputs& inputs) noexcept
    -> HumanoidLocomotionPhaseOverrideSample {
  if (!inputs.bow_ready_idle || inputs.has_locomotion || inputs.attacking) {
    return {};
  }
  return {
      .active = true,
      .cycle_phase = 0.5F,
  };
}

auto resolve_humanoid_locomotion_variation(
    const HumanoidLocomotionVariationInputs& inputs) noexcept
    -> HumanoidLocomotionVariationSample {
  HumanoidLocomotionVariationSample sample{
      .walk_speed_multiplier = inputs.walk_speed_multiplier,
      .arm_swing_amplitude = inputs.arm_swing_amplitude,
      .stance_width = inputs.stance_width,
      .posture_slump = inputs.posture_slump,
  };

  if (inputs.blend_from_presence) {

    float const run = std::clamp(inputs.run_presence, 0.0F, 1.0F);
    float const walk =
        std::clamp(inputs.locomotion_presence, 0.0F, 1.0F) * (1.0F - run);
    sample.walk_speed_multiplier *= 1.0F + (0.25F * run) + (0.05F * walk);
    sample.arm_swing_amplitude *= 1.0F + (0.12F * run);
    sample.stance_width *= 1.0F - (0.04F * run);
    sample.posture_slump = std::min(0.16F, sample.posture_slump + (0.020F * run));
  } else if (inputs.running) {
    sample.walk_speed_multiplier *= 1.25F;
    sample.arm_swing_amplitude *= 1.12F;
    sample.stance_width *= 0.96F;
    sample.posture_slump = std::min(0.16F, sample.posture_slump + 0.020F);
  } else if (inputs.has_locomotion) {
    sample.walk_speed_multiplier *= 1.05F;
  }

  return sample;
}

auto resolve_humanoid_base_pose(const HumanoidBasePoseInputs& inputs) noexcept
    -> HumanoidBasePoseSample {
  auto const& hp = inputs.proportions;
  float const h_scale = inputs.height_scale;
  float const b_scale = inputs.bulk_scale;
  float const s_width = inputs.stance_width;
  float const shoulder_span = hp.shoulder_width * (1.00F + (b_scale - 1.0F) * 0.10F);
  float const half_shoulder_span = 0.5F * shoulder_span;
  float const chest_forward = 0.014F + inputs.posture_slump * 0.08F;
  float const pelvis_setback = -0.010F * h_scale;

  HumanoidBasePoseSample sample{};
  sample.head_pos = {0.0F, hp.head_center_y * h_scale, chest_forward * 0.55F};
  sample.head_radius = hp.head_radius * h_scale;
  sample.neck_base = {0.0F, hp.neck_base_y * h_scale, chest_forward * 0.35F};
  sample.shoulder_l = {-half_shoulder_span, hp.shoulder_y * h_scale, chest_forward};
  sample.shoulder_r = {half_shoulder_span, hp.shoulder_y * h_scale, chest_forward};
  sample.pelvis = {0.0F, hp.waist_y * h_scale - 0.018F * h_scale, pelvis_setback};

  float const rest_stride = 0.018F + (s_width - 1.0F) * 0.024F;
  float const foot_x_span = shoulder_span * 0.40F * s_width;
  sample.foot_y_offset = hp.foot_y_offset_default;
  sample.foot_l = {-foot_x_span, hp.ground_y + sample.foot_y_offset, rest_stride};
  sample.foot_r = {foot_x_span, hp.ground_y + sample.foot_y_offset, -rest_stride};

  sample.shoulder_l.y += inputs.shoulder_tilt;
  sample.shoulder_r.y -= inputs.shoulder_tilt;

  float const slouch_offset = inputs.posture_slump * 0.15F;
  sample.shoulder_l.z += slouch_offset;
  sample.shoulder_r.z += slouch_offset;
  sample.neck_base.z += slouch_offset * 0.85F;
  sample.head_pos.z += slouch_offset * 0.65F;

  float const foot_inward_jitter = (hash_01(inputs.seed ^ 0x5678U) - 0.5F) * 0.008F;
  float const foot_forward_jitter = (hash_01(inputs.seed ^ 0x9ABCU) - 0.5F) * 0.016F;
  sample.foot_l.x += foot_inward_jitter;
  sample.foot_r.x -= foot_inward_jitter;
  sample.foot_l.z += foot_forward_jitter;
  sample.foot_r.z -= foot_forward_jitter;

  float const arm_height_jitter = (hash_01(inputs.seed ^ 0xABCDU) - 0.5F) * 0.016F;
  float const arm_asymmetry = (hash_01(inputs.seed ^ 0xDEF0U) - 0.5F) * 0.018F;
  float const relaxed_hand_y = (hp.shoulder_y * h_scale) - (0.545F * h_scale) +
                               arm_height_jitter + (inputs.posture_slump * 0.02F);
  float const hand_forward = 0.030F + inputs.posture_slump * 0.04F;
  sample.hand_l = {-half_shoulder_span * 0.62F - 0.022F + arm_asymmetry,
                   relaxed_hand_y,
                   hand_forward + 0.010F};
  sample.hand_r = {half_shoulder_span * 0.62F + 0.022F - arm_asymmetry * 0.60F,
                   relaxed_hand_y + 0.010F,
                   hand_forward - 0.010F};
  return sample;
}

auto resolve_humanoid_locomotion_pose(
    const HumanoidLocomotionPoseInputs& inputs) noexcept
    -> HumanoidLocomotionPoseSample {
  bool const is_moving = inputs.state == HumanoidMotionState::Walk ||
                         inputs.state == HumanoidMotionState::Run;
  if (!is_moving) {
    return {};
  }

  float const speed_factor = std::clamp(
      inputs.normalized_speed > 0.0F ? inputs.normalized_speed : 1.0F, 0.75F, 1.20F);
  float const locomotion_blend =
      std::clamp((inputs.locomotion_blend > 0.0F) ? inputs.locomotion_blend
                                                  : inputs.normalized_speed,
                 0.0F,
                 1.0F);
  float const run_blend =
      std::clamp((inputs.run_blend > 0.0F)
                     ? inputs.run_blend
                     : (inputs.state == HumanoidMotionState::Run ? 1.0F : 0.0F),
                 0.0F,
                 1.0F);
  LocomotionPoseProfile const profile =
      profile_for_pose(inputs, speed_factor, run_blend);
  float const stride_distance_scale_factor = stride_distance_scale(inputs, run_blend);
  float const walk_phase = wrap_locomotion_phase(inputs.cycle_phase);
  float const left_phase = walk_phase;
  float const right_phase = wrap_locomotion_phase(walk_phase + 0.5F);

  float const cycle_radians = walk_phase * 2.0F * std::numbers::pi_v<float>;
  float const acceleration = std::clamp(inputs.acceleration, -2.0F, 2.0F);
  float const acceleration_push = std::max(0.0F, acceleration);
  float const braking = std::max(0.0F, -acceleration);
  float const turn_amount = std::clamp(inputs.turn_amount, -1.0F, 1.0F);
  float const turn_abs = std::abs(turn_amount);
  float const travel_alignment = std::clamp(inputs.travel_alignment, -1.0F, 1.0F);

  float const travel_stride_scale =
      lerp(k_reverse_stride_scale, 1.0F, (travel_alignment + 1.0F) * 0.5F);
  float const stride_scale =
      std::max(0.10F,
               locomotion_blend * travel_stride_scale *
                   (1.0F + acceleration_push * 0.12F - braking * 0.16F));
  float const step_scale = std::max(0.15F,
                                    locomotion_blend * travel_stride_scale *
                                        (1.0F + acceleration_push * 0.08F));
  float const planted_fraction =
      std::clamp(profile.planted_fraction + braking * 0.06F -
                     acceleration_push * 0.04F + turn_abs * 0.04F,
                 0.34F,
                 0.74F);
  auto bob_at = [&](float phase_lag) {
    float const phase_lag_radians = phase_lag * 2.0F * std::numbers::pi_v<float>;
    float const walk_wave =
        std::abs(std::sin(cycle_radians - phase_lag_radians)) - k_abs_sine_mean;
    float const run_wave =
        k_abs_sine_mean -
        std::abs(std::sin(cycle_radians +
                          k_run_bob_phase_advance * 2.0F * std::numbers::pi_v<float> -
                          phase_lag_radians));
    return lerp(walk_wave, run_wave, run_blend) * profile.vertical_bob * stride_scale;
  };
  float const vertical_bob = bob_at(0.0F);
  float const shoulder_bob = bob_at(k_shoulder_bob_lag_phase);
  float const neck_bob = bob_at(k_neck_bob_lag_phase);
  float const head_bob = bob_at(k_head_bob_lag_phase);

  float const sway_raw = std::sin(cycle_radians);
  float const sway_lagged =
      std::sin(cycle_radians - k_torso_lag_phase * 2.0F * std::numbers::pi_v<float>);
  float const hip_sway = -sway_raw * profile.hip_sway * stride_scale;
  float const shoulder_sway = -sway_lagged * profile.shoulder_sway * stride_scale;

  float const torso_basis = std::sin((walk_phase - k_arm_swing_phase_shift) * 2.0F *
                                     std::numbers::pi_v<float>);
  float const shoulder_twist = torso_basis * profile.shoulder_twist * stride_scale;
  float const pelvis_twist = -torso_basis * profile.pelvis_twist * stride_scale;
  float const acceleration_lean = acceleration * 0.006F;
  float const braking_sink = braking * 0.010F * locomotion_blend;
  float const forward_lean =
      profile.forward_lean * stride_scale * travel_alignment + acceleration_lean;
  float const turn_lean = turn_amount * (0.010F + 0.012F * locomotion_blend);
  float const turn_twist = turn_amount * (0.008F + 0.010F * locomotion_blend);
  float const turn_step_bias = turn_amount * 0.018F * stride_scale;

  auto support_amount = [planted_fraction](float phase) {
    if (phase < planted_fraction) {
      float const t = phase / planted_fraction;
      return 1.0F - smooth_pulse(t, 0.70F, 0.88F, 1.0F);
    }
    float const t = (phase - planted_fraction) / (1.0F - planted_fraction);
    return smooth_pulse(t, 0.82F, 0.94F, 1.0F) * 0.18F;
  };
  auto flight_amount = [planted_fraction](float phase) {
    float const half = std::fmod(phase, 0.5F);
    if (half < planted_fraction || planted_fraction >= 0.5F) {
      return 0.0F;
    }
    float const t = (half - planted_fraction) / (0.5F - planted_fraction);
    return std::sin(std::clamp(t, 0.0F, 1.0F) * std::numbers::pi_v<float>);
  };
  float const flight_lift =
      flight_amount(walk_phase) * k_run_flight_lift * step_scale * run_blend;
  float const left_support = support_amount(left_phase);
  float const right_support = support_amount(right_phase);
  float const support_shift = std::clamp(right_support - left_support, -1.0F, 1.0F);
  float const weight_shift = support_shift * profile.weight_shift * stride_scale;
  float const double_support = std::min(left_support, right_support);
  float const loaded_support = std::max(left_support, right_support);
  float const support_compression = lerp(double_support, loaded_support, run_blend);
  float const pelvis_drop = support_compression * profile.pelvis_drop * step_scale;
  float const torso_support_drop = pelvis_drop * lerp(0.62F, 0.86F, run_blend);
  float const head_counter_bob =
      -vertical_bob * profile.head_stabilization * (0.45F + 0.10F * run_blend);

  HumanoidLocomotionPoseSample sample{};
  sample.active = true;
  sample.foot_l = resolve_locomotion_foot(inputs,
                                          profile,
                                          left_phase,
                                          -1.0F,
                                          planted_fraction,
                                          stride_scale,
                                          step_scale,
                                          locomotion_blend,
                                          stride_distance_scale_factor,
                                          acceleration_push,
                                          braking,
                                          turn_amount,
                                          turn_abs,
                                          turn_step_bias,
                                          weight_shift,
                                          inputs.base_foot_l,
                                          sample.foot_pitch_l);
  sample.foot_r = resolve_locomotion_foot(inputs,
                                          profile,
                                          right_phase,
                                          1.0F,
                                          planted_fraction,
                                          stride_scale,
                                          step_scale,
                                          locomotion_blend,
                                          stride_distance_scale_factor,
                                          acceleration_push,
                                          braking,
                                          turn_amount,
                                          turn_abs,
                                          turn_step_bias,
                                          weight_shift,
                                          inputs.base_foot_r,
                                          sample.foot_pitch_r);

  float stride_hip_drop = 0.0F;
  if (inputs.leg_length > 1.0e-3F && inputs.pelvis_y > 1.0e-3F) {

    float const hip_y = inputs.pelvis_y + inputs.hip_vertical_offset;
    float const reach = inputs.leg_length * 0.995F;
    auto required_drop = [&](const PoseVec3& foot, float lateral_sign) {
      float const dx = foot.x - (lateral_sign * inputs.hip_lateral_offset);
      float const dz = foot.z;
      float const horizontal_sq = (dx * dx) + (dz * dz);
      if (horizontal_sq >= reach * reach) {
        return hip_y - foot.y;
      }
      float const vertical = std::sqrt((reach * reach) - horizontal_sq);
      return std::max(0.0F, (hip_y - foot.y) - vertical);
    };
    stride_hip_drop = std::max(required_drop(sample.foot_l, -1.0F),
                               required_drop(sample.foot_r, 1.0F));
  }

  sample.pelvis_delta.y +=
      vertical_bob + flight_lift - braking_sink - pelvis_drop - stride_hip_drop;
  sample.shoulder_l_delta.y += shoulder_bob * 0.45F + flight_lift -
                               braking_sink * 0.20F - torso_support_drop -
                               stride_hip_drop;
  sample.shoulder_r_delta.y += shoulder_bob * 0.45F + flight_lift -
                               braking_sink * 0.20F - torso_support_drop -
                               stride_hip_drop;
  sample.neck_delta.y += neck_bob * 0.28F + flight_lift + head_counter_bob * 0.55F -
                         torso_support_drop * 0.84F - stride_hip_drop;
  sample.head_delta.y +=
      head_bob * 0.18F + flight_lift + acceleration_push * 0.003F * locomotion_blend +
      head_counter_bob - torso_support_drop * 0.68F - stride_hip_drop;
  sample.foot_l.y += flight_lift;
  sample.foot_r.y += flight_lift;

  float const pelvis_lateral = hip_sway + weight_shift;
  float const torso_lateral = shoulder_sway + weight_shift * 0.45F;
  float const head_lateral = pelvis_lateral * (1.0F - profile.head_stabilization);

  sample.pelvis_delta.x += pelvis_lateral + turn_lean * 0.55F;
  sample.shoulder_l_delta.x += torso_lateral + turn_lean;
  sample.shoulder_r_delta.x += torso_lateral + turn_lean;
  sample.neck_delta.x += lerp(torso_lateral, head_lateral, 0.55F);
  sample.head_delta.x += head_lateral + turn_lean * 0.40F;

  sample.pelvis_delta.z += forward_lean * 0.20F;
  sample.shoulder_l_delta.z += forward_lean + shoulder_twist + turn_twist;
  sample.shoulder_r_delta.z += forward_lean - shoulder_twist - turn_twist;
  sample.neck_delta.z += forward_lean * 0.78F;
  sample.head_delta.z += forward_lean * 0.68F - braking * 0.004F * locomotion_blend;

  sample.hip_l_delta.z += pelvis_twist;
  sample.hip_r_delta.z -= pelvis_twist;
  float const hip_list = profile.pelvis_drop * 0.90F * step_scale;
  sample.hip_l_delta.y -= support_shift * hip_list * 0.5F;
  sample.hip_r_delta.y += support_shift * hip_list * 0.5F;

  float const arm_length = std::max(0.20F, inputs.arm_pendulum_length);
  auto apply_arm_swing = [&](PoseVec3& hand_delta, float phase, float lateral_sign) {
    float const raw =
        std::sin((phase - k_arm_swing_phase_shift) * 2.0F * std::numbers::pi_v<float>);
    float const bias = (raw >= 0.0F) ? k_arm_forward_bias : k_arm_backward_bias;
    float const forward =
        std::clamp(raw * bias * profile.arm_swing,
                   -profile.max_arm_displacement * k_arm_backward_bias,
                   profile.max_arm_displacement * k_arm_forward_bias) *
        stride_scale;
    float const angle = std::asin(std::clamp(forward / arm_length, -0.85F, 0.85F));
    float const flex = profile.elbow_flex * (0.55F + 0.45F * std::max(0.0F, raw));
    float const extension = arm_length * (1.0F - flex);
    hand_delta.z += extension * std::sin(angle);
    hand_delta.y += arm_length - extension * std::cos(angle);
    hand_delta.x -= lateral_sign * forward * profile.arm_counter_shift;
  };
  apply_arm_swing(sample.hand_l_delta, left_phase, -1.0F);
  apply_arm_swing(sample.hand_r_delta, right_phase, 1.0F);
  sample.hand_l_delta.y += flight_lift - stride_hip_drop;
  sample.hand_r_delta.y += flight_lift - stride_hip_drop;
  sample.hand_l_delta.x += turn_amount * 0.010F * locomotion_blend;
  sample.hand_r_delta.x += turn_amount * 0.010F * locomotion_blend;
  return sample;
}

} // namespace Animation
