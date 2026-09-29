#include <algorithm>
#include <cmath>

#include "../util/planar_math.h"
#include "movement_trace_troop_detectors.h"

namespace Engine::Core::movement_analysis {

namespace {

auto shortest_angle(float from_degrees, float to_degrees) -> float {
  return Game::Systems::signed_yaw_delta(from_degrees, to_degrees);
}

} // namespace

void HeadingDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  auto const& sample = frame.sample;
  if (m_has_previous_yaw) {
    float const frame_flip =
        sample.about_faced != m_previous_about_faced ? 180.0F : 0.0F;
    float const delta = shortest_angle(m_previous_yaw + frame_flip, sample.root_yaw);
    observe_angular_rates(frame, env, delta);
    observe_oscillation(frame, env, delta);
  }
  m_previous_yaw = sample.root_yaw;
  m_previous_about_faced = sample.about_faced;
  m_has_previous_yaw = true;
}

void HeadingDetector::observe_angular_rates(const SampleFrame& frame,
                                            const DetectorEnv& env,
                                            float delta) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;
  float const angular_speed = std::fabs(delta) / frame.dt;
  if (angular_speed > thresholds.max_angular_speed_degrees) {
    env.report(MovementFindingKind::AngularSpeedExceeded,
               sample,
               angular_speed,
               text("%.0f deg/s body yaw rate", static_cast<double>(angular_speed)));
  }
  if (m_has_previous_angular_speed) {
    float const angular_accel =
        std::fabs(angular_speed - m_previous_angular_speed) / frame.dt;
    if (angular_accel > thresholds.max_angular_acceleration_degrees) {
      env.report(MovementFindingKind::AngularAccelerationExceeded,
                 sample,
                 angular_accel,
                 text("%.0f deg/s^2 body yaw acceleration",
                      static_cast<double>(angular_accel)));
    }
  }
  m_previous_angular_speed = angular_speed;
  m_has_previous_angular_speed = true;
}

void HeadingDetector::observe_oscillation(const SampleFrame& frame,
                                          const DetectorEnv& env,
                                          float delta) {
  auto const& thresholds = env.thresholds;
  bool const significant = std::fabs(delta) > thresholds.heading_flip_degrees;
  bool const flipped =
      significant && m_previous_heading_delta != 0.0F &&
      std::signbit(delta) != std::signbit(m_previous_heading_delta) &&
      std::fabs(m_previous_heading_delta) > thresholds.heading_flip_degrees;
  if (flipped) {
    ++m_heading_flip_run;
    ++env.summary.heading_flips;
    if (m_heading_flip_run >= thresholds.heading_flip_run_length) {
      env.report(MovementFindingKind::HeadingOscillation,
                 frame.sample,
                 std::fabs(delta),
                 text("%d consecutive alternating heading steps, last %.1f deg",
                      m_heading_flip_run,
                      static_cast<double>(delta)));
      m_heading_flip_run = 0;
    }
  } else if (significant) {
    m_heading_flip_run = 0;
  }
  if (significant) {
    m_previous_heading_delta = delta;
  }
}

void DirectionReversalDetector::on_new_order() {
  m_reversal_ticks.clear();
}

void DirectionReversalDetector::observe(const SampleFrame& frame,
                                        const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;
  if (!(frame.speed > thresholds.reversal_min_speed)) {
    return;
  }

  float const direction =
      std::atan2(sample.accepted_vx, sample.accepted_vz) * 180.0F / 3.14159265F;
  if (m_has_previous_direction) {
    float const change =
        std::fabs(shortest_angle(m_previous_direction_degrees, direction));
    if (change > thresholds.reversal_degrees && !frame.new_order) {
      m_reversal_ticks.push_back(sample.tick);
      auto const window_ticks = static_cast<std::uint64_t>(
          std::max(1.0F, thresholds.reversal_window_seconds / env.step));
      std::erase_if(m_reversal_ticks, [&](std::uint64_t recorded) {
        return sample.tick - recorded > window_ticks;
      });
      if (static_cast<int>(m_reversal_ticks.size()) > thresholds.reversal_allowance) {
        env.report(MovementFindingKind::DirectionReversal,
                   sample,
                   change,
                   text("%zu direction reversals over %.0f deg in %.2fs",
                        m_reversal_ticks.size(),
                        static_cast<double>(thresholds.reversal_degrees),
                        static_cast<double>(thresholds.reversal_window_seconds)));
        m_reversal_ticks.clear();
      }
    }
  }
  m_previous_direction_degrees = direction;
  m_has_previous_direction = true;
}

void GaitDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;

  if (sample.presentation_valid && frame.locomotion &&
      frame.speed < thresholds.gait_stopped_speed) {
    m_stopped_seconds += frame.dt;
    if (m_stopped_seconds > thresholds.gait_mismatch_seconds) {
      env.report_run(m_without_motion_run,
                     MovementFindingKind::GaitWithoutMotion,
                     sample,
                     m_stopped_seconds,
                     text("locomotion state %u held %.2fs at %.3f m/s accepted",
                          static_cast<unsigned>(sample.presentation_state),
                          static_cast<double>(m_stopped_seconds),
                          static_cast<double>(frame.speed)));
    }
  } else {
    m_stopped_seconds = 0.0F;
    FindingSink::close(m_without_motion_run);
  }

  if (sample.presentation_valid && state_claims_stillness(sample.presentation_state) &&
      frame.speed > thresholds.gait_moving_speed) {
    m_moving_seconds += frame.dt;
    if (m_moving_seconds > thresholds.gait_mismatch_seconds) {
      env.report_run(m_idle_moving_run,
                     MovementFindingKind::IdleWhileMoving,
                     sample,
                     m_moving_seconds,
                     text("idle held %.2fs at %.3f m/s accepted",
                          static_cast<double>(m_moving_seconds),
                          static_cast<double>(frame.speed)));
    }
  } else {
    m_moving_seconds = 0.0F;
    FindingSink::close(m_idle_moving_run);
  }

  if (sample.presentation_valid && frame.locomotion &&
      sample.direction_source == MovementDirectionSource::DesiredVelocity) {
    env.report(MovementFindingKind::DirectionSourceNotAccepted,
               sample,
               frame.speed,
               "gait direction taken from desired velocity, not accepted motion");
  }
}

void ArrivalDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;

  if (sample.state == MovementOrderState::Arrived) {
    if (!m_pending && m_previous_state != MovementOrderState::Arrived) {
      m_pending = true;
      m_seconds = 0.0F;
    }
    if (m_pending) {
      m_seconds += frame.dt;
      bool const settled =
          frame.speed < thresholds.arrival_settle_speed && !frame.locomotion;
      if (settled) {
        m_pending = false;
      } else if (m_seconds > thresholds.arrival_settle_seconds) {
        env.report(MovementFindingKind::ArrivalNotSettled,
                   sample,
                   frame.speed,
                   text("%.2fs after arrival still %.3f m/s, locomotion %u",
                        static_cast<double>(m_seconds),
                        static_cast<double>(frame.speed),
                        static_cast<unsigned>(sample.presentation_state)));
        m_pending = false;
      }
    }
  } else if (m_previous_state == MovementOrderState::Arrived && frame.active &&
             !frame.new_order) {
    env.report(MovementFindingKind::ArrivalRestart,
               sample,
               0.0F,
               text("order restarted into %s without a new command",
                    movement_state_name(sample.state)));
  }
  m_previous_state = sample.state;
}

void LayoutDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  auto const& sample = frame.sample;
  if (m_has_previous_mode && sample.traversal_mode != m_previous_mode) {
    observe_mode_change(frame, env);
  } else {
    m_mode_seconds += frame.dt;
  }
  if (sample.portal_id != m_previous_portal) {
    m_mode_changes = 0;
  }
  m_previous_mode = sample.traversal_mode;
  m_has_previous_mode = true;
  m_previous_portal = sample.portal_id;

  observe_aspect_ratio(frame, env);
}

void LayoutDetector::observe_mode_change(const SampleFrame& frame,
                                         const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;
  ++m_mode_changes;
  ++env.summary.layout_transitions;
  if (m_mode_seconds < thresholds.layout_min_dwell_seconds) {
    env.report(MovementFindingKind::LayoutModeDwellTooShort,
               sample,
               m_mode_seconds,
               text("%s held only %.2fs before %s",
                    traversal_layout_mode_name(m_previous_mode),
                    static_cast<double>(m_mode_seconds),
                    traversal_layout_mode_name(sample.traversal_mode)));
  }
  m_mode_seconds = 0.0F;
  if (sample.portal_id == m_previous_portal &&
      static_cast<int>(m_mode_changes) > thresholds.layout_toggle_allowance) {
    env.report(MovementFindingKind::LayoutModeToggle,
               sample,
               static_cast<float>(m_mode_changes),
               text("%u traversal-mode changes inside portal %u",
                    m_mode_changes,
                    sample.portal_id));
  }
}

void LayoutDetector::observe_aspect_ratio(const SampleFrame& frame,
                                          const DetectorEnv& env) {
  auto const& sample = frame.sample;
  if (sample.current_files == 1U && sample.normal_files > 1U &&
      sample.file_spacing > 0.01F &&
      sample.corridor_half_width >
          sample.soldier_body_radius + (0.5F * sample.file_spacing)) {
    env.report(MovementFindingKind::LayoutAspectRatio,
               sample,
               sample.corridor_half_width,
               text("single file chosen where %.2fm of corridor holds two "
                    "files %.2fm apart",
                    static_cast<double>(sample.corridor_half_width),
                    static_cast<double>(sample.file_spacing)));
  }
}

} // namespace Engine::Core::movement_analysis
