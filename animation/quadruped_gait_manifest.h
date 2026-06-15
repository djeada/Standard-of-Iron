#pragma once

#include <array>
#include <cstdint>

#include "pose_manifest.h"

namespace Animation {

enum class QuadrupedLegId : std::uint8_t {
  FrontLeft = 0,
  FrontRight = 1,
  BackLeft = 2,
  BackRight = 3,
};

inline constexpr std::array<QuadrupedLegId, 4> k_quadruped_legs{
    QuadrupedLegId::FrontLeft,
    QuadrupedLegId::FrontRight,
    QuadrupedLegId::BackLeft,
    QuadrupedLegId::BackRight,
};

struct QuadrupedDimensions {
  float body_width{1.0F};
  float body_height{1.0F};
  float body_length{1.0F};
  float barrel_center_y{0.0F};
  float idle_bob_amplitude{0.01F};
  float move_bob_amplitude{0.03F};
};

struct QuadrupedGait {
  float cycle_time{1.0F};
  float stride_swing{0.0F};
  float stride_lift{0.0F};
  float front_leg_phase{0.0F};
  float rear_leg_phase{0.5F};
  float phase_offset{0.0F};
};

struct QuadrupedMotionConfig {
  float idle_phase_speed{0.30F};
  float idle_bob_frequency{0.45F};
  float cycle_time_floor{0.001F};
  float moving_primary_weight{0.70F};
  float moving_secondary_phase{0.19F};
  float moving_secondary_frequency{2.0F};
  float moving_secondary_weight{0.30F};
  float moving_tertiary_phase{0.0F};
  float moving_tertiary_frequency{1.0F};
  float moving_tertiary_weight{0.0F};
  float moving_bob_base_scale{1.0F};
  float moving_bob_intensity_scale{0.0F};
  float running_bob_scale{1.10F};

  float stride_lift_intensity_scale{2.20F};
  float running_intensity_bonus{0.22F};
  float base_intensity{0.62F};
  float swing_intensity_scale{0.24F};
  float lift_intensity_scale{0.14F};
};

struct QuadrupedSwayConfig {
  float idle_frequency{0.30F};
  float idle_amplitude{0.008F};
  float moving_secondary_phase{0.17F};
  float moving_secondary_weight{0.35F};
  float base_amplitude{0.008F};
  float stride_scale{0.0035F};
};

struct QuadrupedMotionSample {
  float phase{0.0F};
  float bob{0.0F};
  float locomotion_intensity{0.0F};
  float body_sway{0.0F};
  bool is_moving{false};
  bool is_fighting{false};
  PoseVec3 barrel_center{};
};

[[nodiscard]] constexpr auto
quadruped_leg_is_front(QuadrupedLegId leg) noexcept -> bool {
  return leg == QuadrupedLegId::FrontLeft || leg == QuadrupedLegId::FrontRight;
}

[[nodiscard]] constexpr auto
quadruped_leg_is_left(QuadrupedLegId leg) noexcept -> bool {
  return leg == QuadrupedLegId::FrontLeft || leg == QuadrupedLegId::BackLeft;
}

[[nodiscard]] constexpr auto
quadruped_leg_lateral_sign(QuadrupedLegId leg) noexcept -> float {
  return quadruped_leg_is_left(leg) ? 1.0F : -1.0F;
}

[[nodiscard]] constexpr auto
quadruped_leg_forward_sign(QuadrupedLegId leg) noexcept -> float {
  return quadruped_leg_is_front(leg) ? 1.0F : -1.0F;
}

[[nodiscard]] auto wrap_quadruped_phase(float phase) noexcept -> float;
[[nodiscard]] auto quadruped_swing_ease(float t,
                                        bool clamp_input = true) noexcept -> float;
[[nodiscard]] auto quadruped_swing_arc(float t,
                                       bool clamp_input = true) noexcept -> float;

[[nodiscard]] auto quadruped_locomotion_intensity(
    bool is_moving,
    bool is_running,
    const QuadrupedGait& gait,
    const QuadrupedMotionConfig& config = {}) noexcept -> float;

[[nodiscard]] auto
quadruped_default_foot_position(const QuadrupedDimensions& dims,
                                QuadrupedLegId leg,
                                const PoseVec3& barrel_center,
                                float lateral_scale = 0.52F,
                                float vertical_scale = 0.42F,
                                float fore_aft_scale = 0.48F) noexcept -> PoseVec3;

[[nodiscard]] auto
quadruped_swing_target(const QuadrupedDimensions& dims,
                       QuadrupedLegId leg,
                       const PoseVec3& barrel_center,
                       float stride_length,
                       float lateral_scale = 0.52F,
                       float vertical_scale = 0.42F,
                       float fore_aft_scale = 0.48F,
                       bool mirror_stride_by_leg_side = true) noexcept -> PoseVec3;

[[nodiscard]] auto
quadruped_body_sway(bool is_moving,
                    float phase,
                    float time,
                    float move_intensity,
                    float stride_swing,
                    const QuadrupedSwayConfig& config = {}) noexcept -> float;

[[nodiscard]] auto resolve_quadruped_cycle_motion(
    const QuadrupedDimensions& dims,
    const QuadrupedGait& gait,
    float time,
    bool is_moving,
    bool is_running,
    bool is_fighting,
    const QuadrupedMotionConfig& motion = {},
    const QuadrupedSwayConfig& sway = {}) noexcept -> QuadrupedMotionSample;

} // namespace Animation
