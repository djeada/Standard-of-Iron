#include <algorithm>
#include <cmath>

#include "movement_trace_troop_detectors.h"

namespace Engine::Core::movement_analysis {

void StallDetector::on_new_order() {
  m_stall_seconds = 0.0F;
  m_stall_advance = 0.0F;
  FindingSink::close(m_run);
}

void StallDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;

  if (sample.state == MovementOrderState::Turning) {
    m_turning_seconds += frame.dt;
  } else {
    m_turning_seconds = 0.0F;
  }
  bool const turning_hold = sample.state == MovementOrderState::Turning &&
                            m_turning_seconds <= thresholds.max_turning_seconds;
  bool const launching = sample.order_seconds < thresholds.launch_grace_seconds;

  if (!frame.active || is_declared_hold(sample.state) || turning_hold || launching) {
    m_stall_seconds = 0.0F;
    m_stall_advance = 0.0F;
    FindingSink::close(m_run);
    return;
  }

  m_stall_seconds += frame.dt;
  m_stall_advance += sample.route_advance;
  if (m_stall_advance >= thresholds.progress_stall_advance_metres) {
    m_stall_seconds = 0.0F;
    m_stall_advance = 0.0F;
    FindingSink::close(m_run);
  } else if (m_stall_seconds > thresholds.progress_stall_window_seconds) {
    env.summary.max_stall_seconds =
        std::max(env.summary.max_stall_seconds, m_stall_seconds);
    env.report_run(m_run,
                   MovementFindingKind::ProgressStall,
                   sample,
                   m_stall_seconds,
                   text("active for %.2fs with %.3fm route progress (state %s)",
                        static_cast<double>(m_stall_seconds),
                        static_cast<double>(m_stall_advance),
                        movement_state_name(sample.state)));
  }
}

void RouteRegressionDetector::on_new_order() {
  m_has_min_remaining = false;
  FindingSink::close(m_run);
}

void RouteRegressionDetector::observe(const SampleFrame& frame,
                                      const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;

  if (sample.route_revision != m_previous_revision) {
    m_has_min_remaining = false;
    m_regression_seconds = 0.0F;
    FindingSink::close(m_run);
  }
  m_previous_revision = sample.route_revision;

  if (!frame.active) {
    m_has_min_remaining = false;
    m_regression_seconds = 0.0F;
    FindingSink::close(m_run);
    return;
  }

  if (!m_has_min_remaining) {
    m_min_remaining = sample.remaining_arclength;
    m_has_min_remaining = true;
  }
  m_min_remaining = std::min(m_min_remaining, sample.remaining_arclength);
  float const regression = sample.remaining_arclength - m_min_remaining;
  env.summary.max_route_regression =
      std::max(env.summary.max_route_regression, regression);

  if (!(regression > thresholds.route_regression_metres) ||
      is_declared_hold(sample.state)) {
    m_regression_seconds = 0.0F;
    FindingSink::close(m_run);
    return;
  }

  m_regression_seconds += frame.dt;
  if (m_regression_seconds > thresholds.route_regression_seconds) {
    env.report_run(m_run,
                   MovementFindingKind::RouteRegression,
                   sample,
                   regression,
                   text("remaining route grew %.2fm for %.2fs in state %s",
                        static_cast<double>(regression),
                        static_cast<double>(m_regression_seconds),
                        movement_state_name(sample.state)));
  }
}

void ObstructionDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  observe_recovery(frame, env);
  observe_local_block(frame, env);
  observe_rejected_steps(frame, env);
}

void ObstructionDetector::observe_recovery(const SampleFrame& frame,
                                           const DetectorEnv& env) {
  auto const& sample = frame.sample;
  if (sample.state != MovementOrderState::Recovering) {
    m_recovering_seconds = 0.0F;
    FindingSink::close(m_recovery_run);
    return;
  }
  m_recovering_seconds += frame.dt;
  if (m_recovering_seconds > env.thresholds.max_recovering_seconds) {
    env.report_run(m_recovery_run,
                   MovementFindingKind::ObstructionNotEscalated,
                   sample,
                   m_recovering_seconds,
                   text("Recovering for %.2fs without a terminal outcome",
                        static_cast<double>(m_recovering_seconds)));
  }
}

void ObstructionDetector::observe_local_block(const SampleFrame& frame,
                                              const DetectorEnv& env) {
  auto const& sample = frame.sample;
  if (sample.state != MovementOrderState::LocallyBlocked) {
    m_blocked_seconds = 0.0F;
    FindingSink::close(m_block_run);
    return;
  }
  m_blocked_seconds += frame.dt;
  if (m_blocked_seconds > env.thresholds.obstruction_response_seconds) {
    env.report_run(m_block_run,
                   MovementFindingKind::ObstructionNotEscalated,
                   sample,
                   m_blocked_seconds,
                   text("LocallyBlocked for %.2fs without escalation",
                        static_cast<double>(m_blocked_seconds)));
  }
}

void ObstructionDetector::observe_rejected_steps(const SampleFrame& frame,
                                                 const DetectorEnv& env) {
  auto const& sample = frame.sample;
  bool const rejected_step =
      sample.has_contact &&
      std::hypot(sample.accepted_dx, sample.accepted_dz) < 1.0e-4F;
  if (!rejected_step) {
    m_blocked_streak = 0;
    return;
  }
  ++m_blocked_streak;
  if (m_blocked_streak == env.thresholds.blocked_step_streak) {
    env.report(MovementFindingKind::BlockedStepStreak,
               sample,
               static_cast<float>(m_blocked_streak),
               text("%d consecutive rejected steps against the same contact",
                    m_blocked_streak));
  }
}

void BodyContactDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;

  if (sample.penetration_depth > 0.0F) {
    m_penetration_seconds += frame.dt;
    if (m_penetration_seconds > thresholds.collision_recovery_seconds) {
      env.report_run(m_penetration_run,
                     MovementFindingKind::CollisionPenetration,
                     sample,
                     sample.penetration_depth,
                     text("%.3fm penetration held for %.2fs",
                          static_cast<double>(sample.penetration_depth),
                          static_cast<double>(m_penetration_seconds)));
    }
  } else {
    m_penetration_seconds = 0.0F;
    FindingSink::close(m_penetration_run);
  }

  if (sample.body_overlap > thresholds.body_overlap_metres) {
    m_overlap_seconds += frame.dt;
    if (m_overlap_seconds > thresholds.body_overlap_seconds) {
      env.report_run(m_overlap_run,
                     MovementFindingKind::BodyOverlap,
                     sample,
                     sample.body_overlap,
                     text("%.3fm inside another body for %.2fs",
                          static_cast<double>(sample.body_overlap),
                          static_cast<double>(m_overlap_seconds)));
    }
  } else {
    m_overlap_seconds = 0.0F;
    FindingSink::close(m_overlap_run);
  }
}

void RouteChurnDetector::on_new_order() {
  m_repaths_in_order = 0;
  m_waypoint_regressions = 0;
}

void RouteChurnDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  auto const& sample = frame.sample;
  auto const& thresholds = env.thresholds;

  if (sample.repath_count > m_repaths_in_order) {
    m_repaths_in_order = sample.repath_count;
    if (static_cast<int>(m_repaths_in_order) > thresholds.repath_allowance) {
      env.report(MovementFindingKind::RepathChurn,
                 sample,
                 static_cast<float>(m_repaths_in_order),
                 text("%u repaths within one order (reason %s)",
                      m_repaths_in_order,
                      movement_repath_reason_name(sample.repath_reason)));
    }
  }

  if (frame.has_previous && sample.route_id == m_previous_route_id &&
      sample.waypoint_index < m_previous_waypoint_index) {
    ++m_waypoint_regressions;
    if (static_cast<int>(m_waypoint_regressions) >
        thresholds.waypoint_regression_allowance) {
      env.report(MovementFindingKind::WaypointRegression,
                 sample,
                 static_cast<float>(m_waypoint_regressions),
                 text("waypoint index fell %u -> %u on route %llu",
                      m_previous_waypoint_index,
                      sample.waypoint_index,
                      static_cast<unsigned long long>(sample.route_id)));
    }
  }
  m_previous_waypoint_index = sample.waypoint_index;
  m_previous_route_id = sample.route_id;
}

void StarvationDetector::observe(const SampleFrame& frame, const DetectorEnv& env) {
  if (frame.active) {
    m_active_seconds += frame.dt;
  } else {
    m_active_seconds = 0.0F;
  }
  if (m_active_seconds > env.thresholds.starvation_seconds) {
    env.report(MovementFindingKind::Starvation,
               frame.sample,
               m_active_seconds,
               text("order active %.1fs without a terminal outcome",
                    static_cast<double>(m_active_seconds)));
    m_active_seconds = 0.0F;
  }
}

} // namespace Engine::Core::movement_analysis
