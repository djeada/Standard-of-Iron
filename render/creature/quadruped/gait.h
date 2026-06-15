#pragma once

#include <QVector3D>

#include "animation/quadruped_gait_manifest.h"
#include "dimensions.h"

namespace Render::Creature::Quadruped {

struct Gait {
  float cycle_time{1.0F};
  float stride_swing{0.0F};
  float stride_lift{0.0F};
  float front_leg_phase{0.0F};
  float rear_leg_phase{0.5F};
  float phase_offset{0.0F};
};

using MotionConfig = Animation::QuadrupedMotionConfig;
using SwayConfig = Animation::QuadrupedSwayConfig;

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
[[nodiscard]] auto swing_ease(float t, bool clamp_input = true) noexcept -> float;
[[nodiscard]] auto swing_arc(float t, bool clamp_input = true) noexcept -> float;

[[nodiscard]] auto
locomotion_intensity(bool is_moving,
                     bool is_running,
                     const Gait& gait,
                     const MotionConfig& config = {}) noexcept -> float;

[[nodiscard]] auto
default_foot_position(const Dimensions& dims,
                      LegId leg,
                      const QVector3D& barrel_center,
                      float lateral_scale = 0.52F,
                      float vertical_scale = 0.42F,
                      float fore_aft_scale = 0.48F) noexcept -> QVector3D;

[[nodiscard]] auto
swing_target(const Dimensions& dims,
             LegId leg,
             const QVector3D& barrel_center,
             float stride_length,
             float lateral_scale = 0.52F,
             float vertical_scale = 0.42F,
             float fore_aft_scale = 0.48F,
             bool mirror_stride_by_leg_side = true) noexcept -> QVector3D;

[[nodiscard]] auto body_sway(bool is_moving,
                             float phase,
                             float time,
                             float move_intensity,
                             float stride_swing,
                             const SwayConfig& config = {}) noexcept -> float;

[[nodiscard]] auto
evaluate_cycle_motion(const Dimensions& dims,
                      const Gait& gait,
                      float time,
                      bool is_moving,
                      bool is_running,
                      bool is_fighting,
                      const MotionConfig& motion = {},
                      const SwayConfig& sway = {}) noexcept -> MotionSample;

} // namespace Render::Creature::Quadruped
