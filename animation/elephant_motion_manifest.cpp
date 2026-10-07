#include "elephant_motion_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "pose_curve.h"

namespace Animation {

namespace {

constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
constexpr float k_degrees_to_radians = std::numbers::pi_v<float> / 180.0F;

auto wrap01(float value) noexcept -> float {
  float wrapped = std::fmod(value, 1.0F);
  return wrapped < 0.0F ? wrapped + 1.0F : wrapped;
}

auto smoothstep01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto wave(float phase, float harmonic, float lag) noexcept -> float {
  return std::sin((phase * harmonic - lag) * k_two_pi);
}

struct StompKey {
  float phase;
  float rear;
  float front_thigh;
  float front_knee;
  float hind_knee;
  float head;
  float trunk_base;
  float trunk_mid;
  float trunk_tip;
  float ears;
  float tail;
};

constexpr std::array k_stomp_keys{
    StompKey{0.00F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F},
    StompKey{0.06F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, -4.0F, -2.0F, 0.0F, 6.0F, 4.0F},
    StompKey{
        0.24F, -7.0F, -12.0F, 34.0F, 5.0F, -6.0F, -26.0F, -18.0F, -20.0F, 18.0F, 12.0F},
    StompKey{0.38F,
             -13.0F,
             -22.0F,
             56.0F,
             9.0F,
             -11.0F,
             -40.0F,
             -28.0F,
             -32.0F,
             24.0F,
             16.0F},
    StompKey{0.46F, 1.5F, -7.0F, 4.0F, 3.0F, 6.0F, 10.0F, 12.0F, 14.0F, 20.0F, 6.0F},
    StompKey{0.54F, 0.6F, -3.0F, 0.0F, 1.0F, 4.0F, 16.0F, 18.0F, 20.0F, 14.0F, -6.0F},
    StompKey{0.72F, -0.4F, -1.0F, 0.0F, 0.0F, 1.0F, 6.0F, 7.0F, 8.0F, 6.0F, 4.0F},
    StompKey{0.92F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F},
    StompKey{1.00F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F},
};

} // namespace

auto elephant_walk_shape() noexcept -> const ElephantGaitShape& {
  static const ElephantGaitShape shape{};
  return shape;
}

auto elephant_run_shape() noexcept -> const ElephantGaitShape& {
  static const ElephantGaitShape shape{.duty = 0.50F,
                                       .sweep_degrees = 21.0F,
                                       .lift_knee_degrees = 50.0F,
                                       .lift_compensation = 0.46F,
                                       .foot_curl_degrees = 20.0F,
                                       .roll_degrees = 2.0F,
                                       .head_nod_degrees = 3.4F,
                                       .trunk_swing_degrees = 10.0F,
                                       .ear_flap_degrees = 12.0F,
                                       .tail_swing_degrees = 16.0F};
  return shape;
}

auto elephant_leg_phase_offset_lateral(ElephantLeg leg) noexcept -> float {
  switch (leg) {
  case ElephantLeg::BackLeft:
    return 0.0F;
  case ElephantLeg::FrontLeft:
    return 0.25F;
  case ElephantLeg::BackRight:
    return 0.5F;
  case ElephantLeg::FrontRight:
    return 0.75F;
  }
  return 0.0F;
}

auto sample_elephant_leg(const ElephantGaitShape& shape,
                         ElephantLeg leg,
                         float cycle_phase) noexcept -> ElephantLegSample {
  float const q = wrap01(cycle_phase - elephant_leg_phase_offset_lateral(leg));
  float const duty = std::clamp(shape.duty, 0.2F, 0.9F);
  ElephantLegSample sample{};
  float fore_aft = 0.0F;
  float lift = 0.0F;
  if (q < duty) {
    fore_aft = 1.0F - 2.0F * (q / duty);
    sample.in_stance = true;
  } else {
    float const v = (q - duty) / (1.0F - duty);
    fore_aft = -1.0F + 2.0F * smoothstep01(v);
    lift = std::sin(std::numbers::pi_v<float> * v);
    sample.in_stance = false;
  }
  float const reach = std::sin(shape.sweep_degrees * k_degrees_to_radians);
  float const thigh =
      -std::asin(std::clamp(fore_aft * reach, -0.95F, 0.95F)) / k_degrees_to_radians;
  sample.knee_degrees = shape.lift_knee_degrees * lift;
  sample.thigh_degrees = thigh - (sample.knee_degrees * shape.lift_compensation);
  sample.foot_degrees =
      -(sample.thigh_degrees + sample.knee_degrees) + (shape.foot_curl_degrees * lift);
  return sample;
}

auto elephant_stance_sweep(const ElephantGaitShape& shape) noexcept -> float {
  return 2.0F * k_elephant_leg_reach *
         std::sin(shape.sweep_degrees * k_degrees_to_radians);
}

auto elephant_cycle_seconds_for_speed(float local_speed,
                                      bool running) noexcept -> float {
  ElephantGaitShape const& shape =
      running ? elephant_run_shape() : elephant_walk_shape();
  if (local_speed <= 1.0e-3F) {
    return k_elephant_max_cycle_seconds;
  }
  float const cycle = elephant_stance_sweep(shape) / (shape.duty * local_speed);
  return std::clamp(cycle, k_elephant_min_cycle_seconds, k_elephant_max_cycle_seconds);
}

auto elephant_locomotion_secondary(const ElephantGaitShape& shape,
                                   float cycle_phase) noexcept
    -> ElephantSecondaryMotion {
  float const p = wrap01(cycle_phase);
  float const trunk = shape.trunk_swing_degrees;
  ElephantSecondaryMotion motion{};
  motion.body_roll_degrees = shape.roll_degrees * wave(p, 1.0F, 0.125F);
  motion.body_pitch_degrees = 0.5F * wave(p, 2.0F, 0.05F);
  motion.head_pitch_degrees = shape.head_nod_degrees * wave(p, 2.0F, 0.20F);
  motion.head_yaw_degrees = 1.6F * wave(p, 1.0F, 0.10F);
  motion.trunk_yaw_degrees = {trunk * wave(p, 1.0F, 0.15F),
                              trunk * 1.3F * wave(p, 1.0F, 0.27F),
                              trunk * 1.6F * wave(p, 1.0F, 0.39F)};
  motion.trunk_pitch_degrees = {trunk * 0.45F * wave(p, 2.0F, 0.20F),
                                trunk * 0.55F * wave(p, 2.0F, 0.34F),
                                trunk * 0.75F * wave(p, 2.0F, 0.48F)};
  motion.ear_left_degrees = shape.ear_flap_degrees * wave(p, 2.0F, 0.0F);
  motion.ear_right_degrees = -shape.ear_flap_degrees * wave(p, 2.0F, 0.12F);
  motion.tail_degrees = shape.tail_swing_degrees * wave(p, 1.0F, 0.30F);
  return motion;
}

auto elephant_idle_secondary(float idle_phase) noexcept -> ElephantSecondaryMotion {
  float const p = wrap01(idle_phase);
  ElephantSecondaryMotion motion{};
  motion.body_roll_degrees = 0.7F * wave(p, 1.0F, 0.0F);
  motion.body_pitch_degrees = 0.35F * wave(p, 2.0F, 0.15F);
  motion.head_pitch_degrees = 1.4F * wave(p, 1.0F, 0.22F) + 0.6F * wave(p, 3.0F, 0.1F);
  motion.head_yaw_degrees = 2.2F * wave(p, 1.0F, 0.40F);
  motion.trunk_yaw_degrees = {5.0F * wave(p, 1.0F, 0.05F),
                              7.0F * wave(p, 1.0F, 0.17F),
                              10.0F * wave(p, 1.0F, 0.29F)};
  motion.trunk_pitch_degrees = {2.5F * wave(p, 2.0F, 0.10F),
                                4.0F * wave(p, 2.0F, 0.22F),
                                9.0F * wave(p, 2.0F, 0.34F)};
  motion.ear_left_degrees = 6.0F * wave(p, 2.0F, 0.0F) + 3.0F * wave(p, 5.0F, 0.0F);
  motion.ear_right_degrees = -6.0F * wave(p, 2.0F, 0.31F) - 3.0F * wave(p, 5.0F, 0.2F);
  motion.tail_degrees = 9.0F * wave(p, 2.0F, 0.0F);
  return motion;
}

auto resolve_elephant_stomp_pose(float phase) noexcept -> ElephantStompPose {
  float const p = std::clamp(phase, 0.0F, 1.0F);
  auto channel = [&](auto accessor) {
    return sample_pose_channel(k_stomp_keys, p, accessor);
  };
  ElephantStompPose pose{};
  pose.rear_degrees = channel([](auto const& key) { return key.rear; });
  pose.front_thigh_degrees = channel([](auto const& key) { return key.front_thigh; });
  pose.front_knee_degrees = channel([](auto const& key) { return key.front_knee; });
  pose.hind_knee_degrees = channel([](auto const& key) { return key.hind_knee; });
  pose.head_pitch_degrees = channel([](auto const& key) { return key.head; });
  pose.trunk_pitch_degrees = {channel([](auto const& key) { return key.trunk_base; }),
                              channel([](auto const& key) { return key.trunk_mid; }),
                              channel([](auto const& key) { return key.trunk_tip; })};
  pose.ear_spread_degrees = channel([](auto const& key) { return key.ears; });
  pose.tail_degrees = channel([](auto const& key) { return key.tail; });
  return pose;
}

} // namespace Animation
