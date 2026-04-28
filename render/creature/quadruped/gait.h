#pragma once

#include "dimensions.h"

#include <QVector3D>

namespace Render::Creature::Quadruped {

struct Gait {
  float cycle_time{1.0F};
  float stride_swing{0.0F};
  float stride_lift{0.0F};
  float front_leg_phase{0.0F};
  float rear_leg_phase{0.5F};
  float phase_offset{0.0F};
};

struct MotionConfig {
  float idle_phase_speed{0.30F};
  float idle_bob_frequency{0.45F};
  float moving_secondary_phase{0.19F};
  float moving_secondary_weight{0.30F};
  float running_bob_scale{1.10F};

  float stride_lift_intensity_scale{2.20F};
  float running_intensity_bonus{0.22F};
  float base_intensity{0.62F};
  float swing_intensity_scale{0.24F};
  float lift_intensity_scale{0.14F};
};

struct SwayConfig {
  float idle_frequency{0.30F};
  float idle_amplitude{0.008F};
  float moving_secondary_phase{0.17F};
  float moving_secondary_weight{0.35F};
  float base_amplitude{0.008F};
  float stride_scale{0.0035F};
};

struct MotionSample {
  float phase{0.0F};
  float bob{0.0F};
  float locomotion_intensity{0.0F};
  float body_sway{0.0F};
  bool is_moving{false};
  bool is_fighting{false};
  QVector3D barrel_center{};
};

[[nodiscard]] auto wrap_phase(float phase) noexcept -> float;
[[nodiscard]] auto swing_ease(float t) noexcept -> float;
[[nodiscard]] auto swing_arc(float t) noexcept -> float;

[[nodiscard]] auto
locomotion_intensity(bool is_moving, bool is_running, const Gait &gait,
                     const MotionConfig &config = {}) noexcept -> float;

[[nodiscard]] auto
default_foot_position(const Dimensions &dims, LegId leg,
                      const QVector3D &barrel_center,
                      float lateral_scale = 0.52F, float vertical_scale = 0.42F,
                      float fore_aft_scale = 0.48F) noexcept -> QVector3D;

[[nodiscard]] auto
swing_target(const Dimensions &dims, LegId leg, const QVector3D &barrel_center,
             float stride_length, float lateral_scale = 0.52F,
             float vertical_scale = 0.42F,
             float fore_aft_scale = 0.48F) noexcept -> QVector3D;

[[nodiscard]] auto body_sway(bool is_moving, float phase, float time,
                             float move_intensity, float stride_swing,
                             const SwayConfig &config = {}) noexcept -> float;

[[nodiscard]] auto
evaluate_cycle_motion(const Dimensions &dims, const Gait &gait, float time,
                      bool is_moving, bool is_running, bool is_fighting,
                      const MotionConfig &motion = {},
                      const SwayConfig &sway = {}) noexcept -> MotionSample;

} // namespace Render::Creature::Quadruped
