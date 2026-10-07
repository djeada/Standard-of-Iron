#pragma once

#include <array>

namespace Animation {

enum class ElephantLeg : int {
  FrontLeft = 0,
  FrontRight = 1,
  BackLeft = 2,
  BackRight = 3,
};

struct ElephantGaitShape {
  float duty{0.64F};
  float sweep_degrees{20.0F};
  float lift_knee_degrees{38.0F};
  float lift_compensation{0.42F};
  float foot_curl_degrees{14.0F};
  float roll_degrees{1.3F};
  float head_nod_degrees{2.2F};
  float trunk_swing_degrees{7.0F};
  float ear_flap_degrees{7.0F};
  float tail_swing_degrees{11.0F};
};

[[nodiscard]] auto elephant_walk_shape() noexcept -> const ElephantGaitShape&;
[[nodiscard]] auto elephant_run_shape() noexcept -> const ElephantGaitShape&;

inline constexpr float k_elephant_leg_reach = 1.0F;
inline constexpr float k_elephant_min_cycle_seconds = 0.80F;
inline constexpr float k_elephant_max_cycle_seconds = 3.40F;
inline constexpr float k_elephant_pivot_cycle_seconds = 1.70F;

[[nodiscard]] auto elephant_leg_phase_offset_lateral(ElephantLeg leg) noexcept -> float;

struct ElephantLegSample {
  float thigh_degrees{0.0F};
  float knee_degrees{0.0F};
  float foot_degrees{0.0F};
  bool in_stance{true};
};

[[nodiscard]] auto sample_elephant_leg(const ElephantGaitShape& shape,
                                       ElephantLeg leg,
                                       float cycle_phase) noexcept -> ElephantLegSample;

[[nodiscard]] auto
elephant_stance_sweep(const ElephantGaitShape& shape) noexcept -> float;

[[nodiscard]] auto elephant_cycle_seconds_for_speed(float local_speed,
                                                    bool running) noexcept -> float;

struct ElephantSecondaryMotion {
  float body_roll_degrees{0.0F};
  float body_pitch_degrees{0.0F};
  float head_pitch_degrees{0.0F};
  float head_yaw_degrees{0.0F};
  std::array<float, 3> trunk_yaw_degrees{};
  std::array<float, 3> trunk_pitch_degrees{};
  float ear_left_degrees{0.0F};
  float ear_right_degrees{0.0F};
  float tail_degrees{0.0F};
};

[[nodiscard]] auto
elephant_locomotion_secondary(const ElephantGaitShape& shape,
                              float cycle_phase) noexcept -> ElephantSecondaryMotion;

[[nodiscard]] auto
elephant_idle_secondary(float idle_phase) noexcept -> ElephantSecondaryMotion;

inline constexpr float k_elephant_stomp_contact_phase = 0.46F;

struct ElephantStompPose {
  float rear_degrees{0.0F};
  float front_thigh_degrees{0.0F};
  float front_knee_degrees{0.0F};
  float hind_knee_degrees{0.0F};
  float head_pitch_degrees{0.0F};
  std::array<float, 3> trunk_pitch_degrees{};
  float ear_spread_degrees{0.0F};
  float tail_degrees{0.0F};
};

[[nodiscard]] auto
resolve_elephant_stomp_pose(float phase) noexcept -> ElephantStompPose;

} // namespace Animation
