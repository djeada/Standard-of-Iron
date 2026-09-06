#include "movement_trace_analysis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

namespace Engine::Core {

namespace {

[[nodiscard]] constexpr auto
state_claims_travel(std::uint8_t presentation_state) -> bool {
  return presentation_state == 2U || presentation_state == 3U ||
         presentation_state == 6U;
}

[[nodiscard]] constexpr auto
state_claims_stillness(std::uint8_t presentation_state) -> bool {
  return presentation_state == 0U;
}

auto shortest_angle(float from_degrees, float to_degrees) -> float {
  return std::fmod((to_degrees - from_degrees + 540.0F), 360.0F) - 180.0F;
}

auto is_active_state(MovementOrderState state) -> bool {
  return is_active_movement_state(state);
}

auto is_declared_hold(MovementOrderState state) -> bool {
  switch (state) {
  case MovementOrderState::Yielding:
  case MovementOrderState::Repathing:
  case MovementOrderState::LocallyBlocked:
  case MovementOrderState::Recovering:
    return true;
  default:
    return false;
  }
}

auto accepted_speed(const MovementTroopSample& sample) -> float {
  return std::hypot(sample.accepted_vx, sample.accepted_vz);
}

struct OpenFinding {
  bool open{false};
  std::size_t index{0};
};

class FindingSink {
public:
  explicit FindingSink(MovementAnalysis& analysis)
      : m_analysis(analysis) {}

  void add(MovementFindingKind kind,
           EntityID entity,
           std::uint32_t slot,
           std::uint64_t tick,
           float magnitude,
           std::string detail) {
    MovementFinding finding;
    finding.kind = kind;
    finding.entity_id = entity;
    finding.slot = slot;
    finding.first_tick = tick;
    finding.last_tick = tick;
    finding.magnitude = magnitude;
    finding.detail = std::move(detail);
    m_analysis.findings.push_back(std::move(finding));
  }

  void extend(OpenFinding& run,
              MovementFindingKind kind,
              EntityID entity,
              std::uint32_t slot,
              std::uint64_t tick,
              float magnitude,
              std::string detail) {
    if (run.open && run.index < m_analysis.findings.size()) {
      auto& existing = m_analysis.findings[run.index];
      existing.last_tick = tick;
      existing.magnitude = std::max(existing.magnitude, magnitude);
      existing.detail = std::move(detail);
      return;
    }
    run.open = true;
    run.index = m_analysis.findings.size();
    add(kind, entity, slot, tick, magnitude, std::move(detail));
  }

  static void close(OpenFinding& run) { run.open = false; }

private:
  MovementAnalysis& m_analysis;
};

template <typename... Args>
auto text(const char* fmt, Args... args) -> std::string {
  std::array<char, 256> buffer{};
  int const written = std::snprintf(buffer.data(), buffer.size(), fmt, args...);
  if (written <= 0) {
    return {};
  }
  return std::string(buffer.data(),
                     std::min(static_cast<std::size_t>(written), buffer.size() - 1U));
}

struct EntityWalkState {
  MovementEntitySummary summary;
  OpenFinding stall;
  OpenFinding regression;
  OpenFinding gait_without_motion;
  OpenFinding idle_while_moving;
  OpenFinding obstruction;
  OpenFinding penetration;
  OpenFinding body_overlap;
  OpenFinding recovery;

  float stall_seconds{0.0F};
  float stall_advance{0.0F};
  float min_remaining{0.0F};
  bool has_min_remaining{false};
  float regression_seconds{0.0F};
  float gait_stopped_seconds{0.0F};
  float gait_moving_seconds{0.0F};
  float blocked_seconds{0.0F};
  float turning_seconds{0.0F};
  float recovering_seconds{0.0F};
  float penetration_seconds{0.0F};
  float body_overlap_seconds{0.0F};
  float active_seconds{0.0F};
  int blocked_streak{0};
  int heading_flip_run{0};
  float previous_heading_delta{0.0F};
  bool has_previous_yaw{false};
  float previous_yaw{0.0F};
  float previous_angular_speed{0.0F};
  bool has_previous_angular_speed{false};
  bool has_previous_direction{false};
  float previous_direction_degrees{0.0F};
  std::vector<std::uint64_t> reversal_ticks;
  std::uint64_t previous_tick{0};
  bool has_previous{false};
  MovementOrderState previous_state{MovementOrderState::Idle};
  std::uint64_t command_sequence{0};
  std::uint32_t repaths_in_order{0};
  std::uint32_t waypoint_regressions{0};
  std::uint32_t previous_waypoint_index{0};
  std::uint64_t previous_route_id{0};
  std::uint64_t previous_route_revision{0};
  TraversalLayoutMode previous_mode{TraversalLayoutMode::Normal};
  bool has_previous_mode{false};
  std::uint32_t mode_changes{0};
  float mode_seconds{0.0F};
  std::uint32_t previous_portal{0};
  bool arrival_pending{false};
  float arrival_seconds{0.0F};
};

} // namespace

auto movement_finding_name(MovementFindingKind kind) noexcept -> const char* {
  switch (kind) {
  case MovementFindingKind::ProgressStall:
    return "ProgressStall";
  case MovementFindingKind::RouteRegression:
    return "RouteRegression";
  case MovementFindingKind::IndefiniteActiveOrder:
    return "IndefiniteActiveOrder";
  case MovementFindingKind::MissingTerminalOutcome:
    return "MissingTerminalOutcome";
  case MovementFindingKind::ObstructionNotEscalated:
    return "ObstructionNotEscalated";
  case MovementFindingKind::BlockedStepStreak:
    return "BlockedStepStreak";
  case MovementFindingKind::RepathChurn:
    return "RepathChurn";
  case MovementFindingKind::WaypointRegression:
    return "WaypointRegression";
  case MovementFindingKind::HeadingOscillation:
    return "HeadingOscillation";
  case MovementFindingKind::DirectionReversal:
    return "DirectionReversal";
  case MovementFindingKind::AngularSpeedExceeded:
    return "AngularSpeedExceeded";
  case MovementFindingKind::AngularAccelerationExceeded:
    return "AngularAccelerationExceeded";
  case MovementFindingKind::ArrivalNotSettled:
    return "ArrivalNotSettled";
  case MovementFindingKind::ArrivalRestart:
    return "ArrivalRestart";
  case MovementFindingKind::GaitWithoutMotion:
    return "GaitWithoutMotion";
  case MovementFindingKind::IdleWhileMoving:
    return "IdleWhileMoving";
  case MovementFindingKind::DirectionSourceNotAccepted:
    return "DirectionSourceNotAccepted";
  case MovementFindingKind::LayoutModeToggle:
    return "LayoutModeToggle";
  case MovementFindingKind::LayoutModeDwellTooShort:
    return "LayoutModeDwellTooShort";
  case MovementFindingKind::LayoutAspectRatio:
    return "LayoutAspectRatio";
  case MovementFindingKind::SlotIdentityChanged:
    return "SlotIdentityChanged";
  case MovementFindingKind::SoldierAnchorJump:
    return "SoldierAnchorJump";
  case MovementFindingKind::MarkerAnchorMismatch:
    return "MarkerAnchorMismatch";
  case MovementFindingKind::ShadowAnchorMismatch:
    return "ShadowAnchorMismatch";
  case MovementFindingKind::PickingAnchorMismatch:
    return "PickingAnchorMismatch";
  case MovementFindingKind::MissingFinalAnchor:
    return "MissingFinalAnchor";
  case MovementFindingKind::CollisionPenetration:
    return "CollisionPenetration";
  case MovementFindingKind::BodyOverlap:
    return "BodyOverlap";
  case MovementFindingKind::Starvation:
    return "Starvation";
  }
  return "Unknown";
}

auto MovementAnalysis::count(MovementFindingKind kind) const -> std::size_t {
  std::size_t total = 0;
  for (auto const& finding : findings) {
    if (finding.kind == kind) {
      ++total;
    }
  }
  return total;
}

auto MovementAnalysis::worst_entity() const -> EntityID {
  EntityID worst = 0;
  std::uint32_t best = 0;
  for (auto const& entity : entities) {
    if (entity.findings > best) {
      best = entity.findings;
      worst = entity.entity_id;
    }
  }
  if (worst == 0 && !entities.empty()) {
    worst = entities.front().entity_id;
  }
  return worst;
}

auto MovementAnalysis::worst_soldier() const -> const MovementSoldierSummary* {
  const MovementSoldierSummary* worst = nullptr;
  float best = -1.0F;
  for (auto const& soldier : soldiers) {
    float const score = soldier.max_marker_error * 1000.0F + soldier.max_anchor_jump;
    if (score > best) {
      best = score;
      worst = &soldier;
    }
  }
  return worst;
}

namespace {

void analyze_troops(const std::vector<MovementTroopSample>& troops,
                    const MovementGateThresholds& thresholds,
                    MovementAnalysis& analysis,
                    FindingSink& sink) {

  std::map<std::pair<std::uint64_t, EntityID>, std::vector<const MovementTroopSample*>>
      by_entity;
  for (auto const& sample : troops) {
    by_entity[{sample.session_id, sample.entity_id}].push_back(&sample);
  }

  float const step = std::max(1.0e-4F, thresholds.fixed_step_seconds);

  for (auto& [key, samples] : by_entity) {
    EntityID const entity_id = key.second;
    std::stable_sort(
        samples.begin(),
        samples.end(),
        [](const MovementTroopSample* lhs, const MovementTroopSample* rhs) {
          return lhs->tick < rhs->tick;
        });

    EntityWalkState walk;
    walk.summary.entity_id = entity_id;
    walk.summary.first_tick = samples.front()->tick;
    std::size_t const findings_before = analysis.findings.size();

    for (auto const* sample_ptr : samples) {
      auto const& sample = *sample_ptr;
      float const dt =
          walk.has_previous
              ? std::max(step,
                         static_cast<float>(sample.tick - walk.previous_tick) * step)
              : step;

      walk.summary.last_tick = sample.tick;
      ++walk.summary.ticks;
      walk.summary.travelled += std::hypot(sample.accepted_dx, sample.accepted_dz);
      walk.summary.blocked_steps =
          std::max(walk.summary.blocked_steps, sample.blocked_steps);
      walk.summary.repaths = std::max(walk.summary.repaths, sample.repath_count);
      walk.summary.final_state = sample.state;

      bool const new_order = sample.command_sequence != walk.command_sequence;
      if (new_order) {
        walk.command_sequence = sample.command_sequence;
        walk.repaths_in_order = 0;
        walk.waypoint_regressions = 0;
        walk.has_min_remaining = false;
        walk.stall_seconds = 0.0F;
        walk.stall_advance = 0.0F;
        walk.active_seconds = 0.0F;
        walk.reversal_ticks.clear();
        FindingSink::close(walk.stall);
        FindingSink::close(walk.regression);
      }

      bool const active = is_active_state(sample.state);
      if (active) {
        walk.active_seconds += dt;
      } else {
        walk.active_seconds = 0.0F;
      }

      if (sample.state == MovementOrderState::Turning) {
        walk.turning_seconds += dt;
      } else {
        walk.turning_seconds = 0.0F;
      }
      bool const turning_hold = sample.state == MovementOrderState::Turning &&
                                walk.turning_seconds <= thresholds.max_turning_seconds;
      bool const launching = sample.order_seconds < thresholds.launch_grace_seconds;

      if (active && !is_declared_hold(sample.state) && !turning_hold && !launching) {
        walk.stall_seconds += dt;
        walk.stall_advance += sample.route_advance;
        if (walk.stall_advance >= thresholds.progress_stall_advance_metres) {
          walk.stall_seconds = 0.0F;
          walk.stall_advance = 0.0F;
          FindingSink::close(walk.stall);
        } else if (walk.stall_seconds > thresholds.progress_stall_window_seconds) {
          walk.summary.max_stall_seconds =
              std::max(walk.summary.max_stall_seconds, walk.stall_seconds);
          sink.extend(walk.stall,
                      MovementFindingKind::ProgressStall,
                      entity_id,
                      0,
                      sample.tick,
                      walk.stall_seconds,
                      text("active for %.2fs with %.3fm route progress (state %s)",
                           static_cast<double>(walk.stall_seconds),
                           static_cast<double>(walk.stall_advance),
                           movement_state_name(sample.state)));
        }
      } else {
        walk.stall_seconds = 0.0F;
        walk.stall_advance = 0.0F;
        FindingSink::close(walk.stall);
      }

      if (sample.route_revision != walk.previous_route_revision) {
        walk.has_min_remaining = false;
        walk.regression_seconds = 0.0F;
        FindingSink::close(walk.regression);
      }
      if (active) {
        if (!walk.has_min_remaining) {
          walk.min_remaining = sample.remaining_arclength;
          walk.has_min_remaining = true;
        }
        walk.min_remaining = std::min(walk.min_remaining, sample.remaining_arclength);
        float const regression = sample.remaining_arclength - walk.min_remaining;
        walk.summary.max_route_regression =
            std::max(walk.summary.max_route_regression, regression);
        if (regression > thresholds.route_regression_metres &&
            !is_declared_hold(sample.state)) {
          walk.regression_seconds += dt;
          if (walk.regression_seconds > thresholds.route_regression_seconds) {
            sink.extend(walk.regression,
                        MovementFindingKind::RouteRegression,
                        entity_id,
                        0,
                        sample.tick,
                        regression,
                        text("remaining route grew %.2fm for %.2fs in state %s",
                             static_cast<double>(regression),
                             static_cast<double>(walk.regression_seconds),
                             movement_state_name(sample.state)));
          }
        } else {
          walk.regression_seconds = 0.0F;
          FindingSink::close(walk.regression);
        }
      } else {
        walk.has_min_remaining = false;
        walk.regression_seconds = 0.0F;
        FindingSink::close(walk.regression);
      }

      if (sample.state == MovementOrderState::Recovering) {
        walk.recovering_seconds += dt;
        if (walk.recovering_seconds > thresholds.max_recovering_seconds) {
          sink.extend(walk.recovery,
                      MovementFindingKind::ObstructionNotEscalated,
                      entity_id,
                      0,
                      sample.tick,
                      walk.recovering_seconds,
                      text("Recovering for %.2fs without a terminal outcome",
                           static_cast<double>(walk.recovering_seconds)));
        }
      } else {
        walk.recovering_seconds = 0.0F;
        FindingSink::close(walk.recovery);
      }

      if (sample.state == MovementOrderState::LocallyBlocked) {
        walk.blocked_seconds += dt;
        if (walk.blocked_seconds > thresholds.obstruction_response_seconds) {
          sink.extend(walk.obstruction,
                      MovementFindingKind::ObstructionNotEscalated,
                      entity_id,
                      0,
                      sample.tick,
                      walk.blocked_seconds,
                      text("LocallyBlocked for %.2fs without escalation",
                           static_cast<double>(walk.blocked_seconds)));
        }
      } else {
        walk.blocked_seconds = 0.0F;
        FindingSink::close(walk.obstruction);
      }

      bool const rejected_step =
          sample.has_contact &&
          std::hypot(sample.accepted_dx, sample.accepted_dz) < 1.0e-4F;
      if (rejected_step) {
        ++walk.blocked_streak;
        if (walk.blocked_streak == thresholds.blocked_step_streak) {
          sink.add(MovementFindingKind::BlockedStepStreak,
                   entity_id,
                   0,
                   sample.tick,
                   static_cast<float>(walk.blocked_streak),
                   text("%d consecutive rejected steps against the same contact",
                        walk.blocked_streak));
        }
      } else {
        walk.blocked_streak = 0;
      }

      if (sample.penetration_depth > 0.0F) {
        walk.penetration_seconds += dt;
        if (walk.penetration_seconds > thresholds.collision_recovery_seconds) {
          sink.extend(walk.penetration,
                      MovementFindingKind::CollisionPenetration,
                      entity_id,
                      0,
                      sample.tick,
                      sample.penetration_depth,
                      text("%.3fm penetration held for %.2fs",
                           static_cast<double>(sample.penetration_depth),
                           static_cast<double>(walk.penetration_seconds)));
        }
      } else {
        walk.penetration_seconds = 0.0F;
        FindingSink::close(walk.penetration);
      }

      if (sample.body_overlap > thresholds.body_overlap_metres) {
        walk.body_overlap_seconds += dt;
        if (walk.body_overlap_seconds > thresholds.body_overlap_seconds) {
          sink.extend(walk.body_overlap,
                      MovementFindingKind::BodyOverlap,
                      entity_id,
                      0,
                      sample.tick,
                      sample.body_overlap,
                      text("%.3fm inside another body for %.2fs",
                           static_cast<double>(sample.body_overlap),
                           static_cast<double>(walk.body_overlap_seconds)));
        }
      } else {
        walk.body_overlap_seconds = 0.0F;
        FindingSink::close(walk.body_overlap);
      }

      if (sample.repath_count > walk.repaths_in_order) {
        walk.repaths_in_order = sample.repath_count;
        if (static_cast<int>(walk.repaths_in_order) > thresholds.repath_allowance) {
          sink.add(MovementFindingKind::RepathChurn,
                   entity_id,
                   0,
                   sample.tick,
                   static_cast<float>(walk.repaths_in_order),
                   text("%u repaths within one order (reason %s)",
                        walk.repaths_in_order,
                        movement_repath_reason_name(sample.repath_reason)));
        }
      }
      if (walk.has_previous && sample.route_id == walk.previous_route_id &&
          sample.waypoint_index < walk.previous_waypoint_index) {
        ++walk.waypoint_regressions;
        if (static_cast<int>(walk.waypoint_regressions) >
            thresholds.waypoint_regression_allowance) {
          sink.add(MovementFindingKind::WaypointRegression,
                   entity_id,
                   0,
                   sample.tick,
                   static_cast<float>(walk.waypoint_regressions),
                   text("waypoint index fell %u -> %u on route %llu",
                        walk.previous_waypoint_index,
                        sample.waypoint_index,
                        static_cast<unsigned long long>(sample.route_id)));
        }
      }

      if (walk.has_previous_yaw) {
        float const delta = shortest_angle(walk.previous_yaw, sample.root_yaw);
        float const angular_speed = std::fabs(delta) / dt;
        if (angular_speed > thresholds.max_angular_speed_degrees) {
          sink.add(
              MovementFindingKind::AngularSpeedExceeded,
              entity_id,
              0,
              sample.tick,
              angular_speed,
              text("%.0f deg/s body yaw rate", static_cast<double>(angular_speed)));
        }
        if (walk.has_previous_angular_speed) {
          float const angular_accel =
              std::fabs(angular_speed - walk.previous_angular_speed) / dt;
          if (angular_accel > thresholds.max_angular_acceleration_degrees) {
            sink.add(MovementFindingKind::AngularAccelerationExceeded,
                     entity_id,
                     0,
                     sample.tick,
                     angular_accel,
                     text("%.0f deg/s^2 body yaw acceleration",
                          static_cast<double>(angular_accel)));
          }
        }
        walk.previous_angular_speed = angular_speed;
        walk.has_previous_angular_speed = true;

        bool const significant = std::fabs(delta) > thresholds.heading_flip_degrees;
        bool const flipped =
            significant && walk.previous_heading_delta != 0.0F &&
            std::signbit(delta) != std::signbit(walk.previous_heading_delta) &&
            std::fabs(walk.previous_heading_delta) > thresholds.heading_flip_degrees;
        if (flipped) {
          ++walk.heading_flip_run;
          ++walk.summary.heading_flips;
          if (walk.heading_flip_run >= thresholds.heading_flip_run_length) {
            sink.add(MovementFindingKind::HeadingOscillation,
                     entity_id,
                     0,
                     sample.tick,
                     std::fabs(delta),
                     text("%d consecutive alternating heading steps, last %.1f deg",
                          walk.heading_flip_run,
                          static_cast<double>(delta)));
            walk.heading_flip_run = 0;
          }
        } else if (significant) {
          walk.heading_flip_run = 0;
        }
        if (significant) {
          walk.previous_heading_delta = delta;
        }
      }
      walk.previous_yaw = sample.root_yaw;
      walk.has_previous_yaw = true;

      float const speed = accepted_speed(sample);
      if (speed > thresholds.reversal_min_speed) {
        float const direction =
            std::atan2(sample.accepted_vx, sample.accepted_vz) * 180.0F / 3.14159265F;
        if (walk.has_previous_direction) {
          float const change =
              std::fabs(shortest_angle(walk.previous_direction_degrees, direction));
          if (change > thresholds.reversal_degrees && !new_order) {
            walk.reversal_ticks.push_back(sample.tick);
            auto const window_ticks = static_cast<std::uint64_t>(
                std::max(1.0F, thresholds.reversal_window_seconds / step));
            std::erase_if(walk.reversal_ticks, [&](std::uint64_t recorded) {
              return sample.tick - recorded > window_ticks;
            });
            if (static_cast<int>(walk.reversal_ticks.size()) >
                thresholds.reversal_allowance) {
              sink.add(MovementFindingKind::DirectionReversal,
                       entity_id,
                       0,
                       sample.tick,
                       change,
                       text("%zu direction reversals over %.0f deg in %.2fs",
                            walk.reversal_ticks.size(),
                            static_cast<double>(thresholds.reversal_degrees),
                            static_cast<double>(thresholds.reversal_window_seconds)));
              walk.reversal_ticks.clear();
            }
          }
        }
        walk.previous_direction_degrees = direction;
        walk.has_previous_direction = true;
      }

      bool const locomotion = state_claims_travel(sample.presentation_state);
      if (sample.presentation_valid && locomotion &&
          speed < thresholds.gait_stopped_speed) {
        walk.gait_stopped_seconds += dt;
        if (walk.gait_stopped_seconds > thresholds.gait_mismatch_seconds) {
          sink.extend(walk.gait_without_motion,
                      MovementFindingKind::GaitWithoutMotion,
                      entity_id,
                      0,
                      sample.tick,
                      walk.gait_stopped_seconds,
                      text("locomotion state %u held %.2fs at %.3f m/s accepted",
                           static_cast<unsigned>(sample.presentation_state),
                           static_cast<double>(walk.gait_stopped_seconds),
                           static_cast<double>(speed)));
        }
      } else {
        walk.gait_stopped_seconds = 0.0F;
        FindingSink::close(walk.gait_without_motion);
      }

      if (sample.presentation_valid &&
          state_claims_stillness(sample.presentation_state) &&
          speed > thresholds.gait_moving_speed) {
        walk.gait_moving_seconds += dt;
        if (walk.gait_moving_seconds > thresholds.gait_mismatch_seconds) {
          sink.extend(walk.idle_while_moving,
                      MovementFindingKind::IdleWhileMoving,
                      entity_id,
                      0,
                      sample.tick,
                      walk.gait_moving_seconds,
                      text("idle held %.2fs at %.3f m/s accepted",
                           static_cast<double>(walk.gait_moving_seconds),
                           static_cast<double>(speed)));
        }
      } else {
        walk.gait_moving_seconds = 0.0F;
        FindingSink::close(walk.idle_while_moving);
      }

      if (sample.presentation_valid && locomotion &&
          sample.direction_source == MovementDirectionSource::DesiredVelocity) {
        sink.add(MovementFindingKind::DirectionSourceNotAccepted,
                 entity_id,
                 0,
                 sample.tick,
                 speed,
                 "gait direction taken from desired velocity, not accepted motion");
      }

      if (sample.state == MovementOrderState::Arrived) {
        if (!walk.arrival_pending &&
            walk.previous_state != MovementOrderState::Arrived) {
          walk.arrival_pending = true;
          walk.arrival_seconds = 0.0F;
        }
        if (walk.arrival_pending) {
          walk.arrival_seconds += dt;
          bool const settled = speed < thresholds.arrival_settle_speed && !locomotion;
          if (settled) {
            walk.arrival_pending = false;
          } else if (walk.arrival_seconds > thresholds.arrival_settle_seconds) {
            sink.add(MovementFindingKind::ArrivalNotSettled,
                     entity_id,
                     0,
                     sample.tick,
                     speed,
                     text("%.2fs after arrival still %.3f m/s, locomotion %u",
                          static_cast<double>(walk.arrival_seconds),
                          static_cast<double>(speed),
                          static_cast<unsigned>(sample.presentation_state)));
            walk.arrival_pending = false;
          }
        }
      } else if (walk.previous_state == MovementOrderState::Arrived && active &&
                 !new_order) {
        sink.add(MovementFindingKind::ArrivalRestart,
                 entity_id,
                 0,
                 sample.tick,
                 0.0F,
                 text("order restarted into %s without a new command",
                      movement_state_name(sample.state)));
      }

      if (walk.has_previous_mode) {
        if (sample.traversal_mode != walk.previous_mode) {
          ++walk.mode_changes;
          ++walk.summary.layout_transitions;
          if (walk.mode_seconds < thresholds.layout_min_dwell_seconds) {
            sink.add(MovementFindingKind::LayoutModeDwellTooShort,
                     entity_id,
                     0,
                     sample.tick,
                     walk.mode_seconds,
                     text("%s held only %.2fs before %s",
                          traversal_layout_mode_name(walk.previous_mode),
                          static_cast<double>(walk.mode_seconds),
                          traversal_layout_mode_name(sample.traversal_mode)));
          }
          walk.mode_seconds = 0.0F;
          if (sample.portal_id == walk.previous_portal &&
              static_cast<int>(walk.mode_changes) >
                  thresholds.layout_toggle_allowance) {
            sink.add(MovementFindingKind::LayoutModeToggle,
                     entity_id,
                     0,
                     sample.tick,
                     static_cast<float>(walk.mode_changes),
                     text("%u traversal-mode changes inside portal %u",
                          walk.mode_changes,
                          sample.portal_id));
          }
        } else {
          walk.mode_seconds += dt;
        }
      } else {
        walk.mode_seconds += dt;
      }
      if (sample.portal_id != walk.previous_portal) {
        walk.mode_changes = 0;
      }
      walk.previous_mode = sample.traversal_mode;
      walk.has_previous_mode = true;
      walk.previous_portal = sample.portal_id;

      if (sample.current_files == 1U && sample.normal_files > 1U &&
          sample.file_spacing > 0.01F &&
          sample.corridor_half_width >
              sample.soldier_body_radius + (0.5F * sample.file_spacing)) {
        sink.add(MovementFindingKind::LayoutAspectRatio,
                 entity_id,
                 0,
                 sample.tick,
                 sample.corridor_half_width,
                 text("single file chosen where %.2fm of corridor holds two "
                      "files %.2fm apart",
                      static_cast<double>(sample.corridor_half_width),
                      static_cast<double>(sample.file_spacing)));
      }

      if (walk.active_seconds > thresholds.starvation_seconds) {
        sink.add(MovementFindingKind::Starvation,
                 entity_id,
                 0,
                 sample.tick,
                 walk.active_seconds,
                 text("order active %.1fs without a terminal outcome",
                      static_cast<double>(walk.active_seconds)));
        walk.active_seconds = 0.0F;
      }

      walk.previous_state = sample.state;
      walk.previous_waypoint_index = sample.waypoint_index;
      walk.previous_route_id = sample.route_id;
      walk.previous_route_revision = sample.route_revision;
      walk.previous_tick = sample.tick;
      walk.has_previous = true;
    }

    const bool ended_while_stalled =
        walk.summary.max_stall_seconds > thresholds.progress_stall_window_seconds;
    if (thresholds.require_terminal_outcomes &&
        is_active_state(walk.summary.final_state) && ended_while_stalled) {
      sink.add(MovementFindingKind::MissingTerminalOutcome,
               entity_id,
               0,
               walk.summary.last_tick,
               0.0F,
               text("trace ends with the order still %s",
                    movement_state_name(walk.summary.final_state)));
    }
    walk.summary.reached_terminal =
        is_terminal_movement_state(walk.summary.final_state) ||
        walk.summary.final_state == MovementOrderState::Idle;
    walk.summary.findings =
        static_cast<std::uint32_t>(analysis.findings.size() - findings_before);
    analysis.entities.push_back(walk.summary);
  }
}

void analyze_soldiers(const std::vector<MovementSoldierSample>& soldiers,
                      const MovementGateThresholds& thresholds,
                      MovementAnalysis& analysis,
                      FindingSink& sink) {
  struct Key {
    std::uint64_t session;
    EntityID troop;
    std::uint32_t slot;
    auto operator<(const Key& other) const -> bool {
      if (session != other.session) {
        return session < other.session;
      }
      return troop != other.troop ? troop < other.troop : slot < other.slot;
    }
  };

  std::map<Key, std::vector<const MovementSoldierSample*>> by_slot;
  std::map<Key, std::vector<std::uint32_t>> per_frame;
  for (auto const& sample : soldiers) {
    by_slot[Key{sample.session_id, sample.troop_id, sample.stable_slot}].push_back(
        &sample);
    per_frame[Key{sample.session_id,
                  sample.troop_id,
                  static_cast<std::uint32_t>(sample.frame)}]
        .push_back(sample.stable_slot);
  }

  for (auto& [frame_key, slots] : per_frame) {
    std::vector<std::uint32_t> sorted = slots;
    std::sort(sorted.begin(), sorted.end());
    auto const duplicate = std::adjacent_find(sorted.begin(), sorted.end());
    if (duplicate != sorted.end()) {
      sink.add(MovementFindingKind::SlotIdentityChanged,
               frame_key.troop,
               *duplicate,
               frame_key.slot,
               0.0F,
               text("slot %u submitted twice in one frame", *duplicate));
    }
  }

  for (auto& [key, samples] : by_slot) {
    std::stable_sort(
        samples.begin(),
        samples.end(),
        [](const MovementSoldierSample* lhs, const MovementSoldierSample* rhs) {
          return lhs->frame < rhs->frame;
        });

    MovementSoldierSummary summary;
    summary.troop_id = key.troop;
    summary.slot = key.slot;
    std::size_t const findings_before = analysis.findings.size();

    const MovementSoldierSample* previous = nullptr;
    for (auto const* sample_ptr : samples) {
      auto const& sample = *sample_ptr;
      ++summary.frames;

      if (!sample.alive || sample.culled) {
        previous = sample_ptr;
        continue;
      }

      if (!sample.has_final_anchor) {
        sink.add(MovementFindingKind::MissingFinalAnchor,
                 key.troop,
                 key.slot,
                 sample.frame,
                 0.0F,
                 "living soldier submitted without a final presented anchor");
      }

      float const ring_error =
          std::max(std::fabs(sample.ring_root_x - sample.body_root_x),
                   std::fabs(sample.ring_root_z - sample.body_root_z));
      summary.max_marker_error = std::max(summary.max_marker_error, ring_error);
      if (ring_error > thresholds.marker_anchor_tolerance) {
        sink.add(MovementFindingKind::MarkerAnchorMismatch,
                 key.troop,
                 key.slot,
                 sample.frame,
                 ring_error,
                 text("ring at (%.5f, %.5f) vs body (%.5f, %.5f)",
                      static_cast<double>(sample.ring_root_x),
                      static_cast<double>(sample.ring_root_z),
                      static_cast<double>(sample.body_root_x),
                      static_cast<double>(sample.body_root_z)));
      }

      float const shadow_error =
          std::max(std::fabs(sample.shadow_root_x - sample.body_root_x),
                   std::fabs(sample.shadow_root_z - sample.body_root_z));
      if (shadow_error > thresholds.marker_anchor_tolerance) {
        sink.add(MovementFindingKind::ShadowAnchorMismatch,
                 key.troop,
                 key.slot,
                 sample.frame,
                 shadow_error,
                 text("contact shadow off the body anchor by %.5f",
                      static_cast<double>(shadow_error)));
      }

      float const picking_error =
          std::max(std::fabs(sample.picking_root_x - sample.body_root_x),
                   std::fabs(sample.picking_root_z - sample.body_root_z));
      if (picking_error > thresholds.marker_anchor_tolerance) {
        sink.add(MovementFindingKind::PickingAnchorMismatch,
                 key.troop,
                 key.slot,
                 sample.frame,
                 picking_error,
                 text("picking proxy off the body anchor by %.5f",
                      static_cast<double>(picking_error)));
      }

      if (previous != nullptr && previous->alive && !previous->culled &&
          sample.frame == previous->frame + 1U) {
        float const jump = std::hypot(sample.body_root_x - previous->body_root_x,
                                      sample.body_root_z - previous->body_root_z);
        summary.max_anchor_jump = std::max(summary.max_anchor_jump, jump);
        if (jump > thresholds.soldier_anchor_jump_metres) {
          sink.add(
              MovementFindingKind::SoldierAnchorJump,
              key.troop,
              key.slot,
              sample.frame,
              jump,
              text("soldier root moved %.3fm in one frame", static_cast<double>(jump)));
        }
      }

      previous = sample_ptr;
    }

    summary.findings =
        static_cast<std::uint32_t>(analysis.findings.size() - findings_before);
    analysis.soldiers.push_back(summary);
  }
}

} // namespace

auto analyze_movement_trace(const std::vector<MovementTroopSample>& troops,
                            const std::vector<MovementSoldierSample>& soldiers,
                            const MovementGateThresholds& thresholds)
    -> MovementAnalysis {
  MovementAnalysis analysis;
  FindingSink sink(analysis);
  analyze_troops(troops, thresholds, analysis, sink);
  analyze_soldiers(soldiers, thresholds, analysis, sink);

  analysis.has_failure = !analysis.findings.empty();
  if (analysis.has_failure) {
    analysis.first_failing_tick = analysis.findings.front().first_tick;
    analysis.first_failing_frame = analysis.findings.front().first_tick;
    for (auto const& finding : analysis.findings) {
      analysis.first_failing_tick =
          std::min(analysis.first_failing_tick, finding.first_tick);
    }
  }
  return analysis;
}

auto analyze_active_movement_trace(const MovementGateThresholds& thresholds)
    -> MovementAnalysis {
  auto& trace = MovementTrace::instance();
  return analyze_movement_trace(
      trace.troop_samples(), trace.soldier_samples(), thresholds);
}

auto format_movement_summary(const MovementAnalysis& analysis) -> std::string {
  std::ostringstream out;
  out << "entity     ticks   travel   stall  regress  repath  blocked  flips  "
         "layout  findings  final\n";
  for (auto const& entity : analysis.entities) {
    out << text("%-10llu %5u %8.2f %7.2f %8.2f %7u %8u %6u %7u %9u  %s\n",
                static_cast<unsigned long long>(entity.entity_id),
                entity.ticks,
                static_cast<double>(entity.travelled),
                static_cast<double>(entity.max_stall_seconds),
                static_cast<double>(entity.max_route_regression),
                entity.repaths,
                entity.blocked_steps,
                entity.heading_flips,
                entity.layout_transitions,
                entity.findings,
                movement_state_name(entity.final_state));
  }
  if (!analysis.soldiers.empty()) {
    out << "\ntroop      slot  frames   maxjump   maxmarker  findings\n";
    for (auto const& soldier : analysis.soldiers) {
      if (soldier.findings == 0U && soldier.max_marker_error <= 0.0F) {
        continue;
      }
      out << text("%-10llu %5u %7u %9.4f %11.6f %9u\n",
                  static_cast<unsigned long long>(soldier.troop_id),
                  soldier.slot,
                  soldier.frames,
                  static_cast<double>(soldier.max_anchor_jump),
                  static_cast<double>(soldier.max_marker_error),
                  soldier.findings);
    }
  }
  return out.str();
}

auto format_movement_findings(const MovementAnalysis& analysis) -> std::string {
  std::ostringstream out;
  for (auto const& finding : analysis.findings) {
    out << text("[%s] entity %llu slot %u ticks %llu..%llu magnitude %.4f: ",
                movement_finding_name(finding.kind),
                static_cast<unsigned long long>(finding.entity_id),
                finding.slot,
                static_cast<unsigned long long>(finding.first_tick),
                static_cast<unsigned long long>(finding.last_tick),
                static_cast<double>(finding.magnitude))
        << finding.detail << '\n';
  }
  return out.str();
}

auto format_movement_timeline(const std::vector<MovementTroopSample>& troops,
                              EntityID entity_id,
                              std::uint64_t centre_tick,
                              std::uint32_t ticks_before,
                              std::uint32_t ticks_after) -> std::string {
  std::uint64_t const low =
      centre_tick > ticks_before ? centre_tick - ticks_before : 0U;
  std::uint64_t const high = centre_tick + ticks_after;

  std::vector<const MovementTroopSample*> window;
  for (auto const& sample : troops) {
    if (sample.entity_id != entity_id || sample.tick < low || sample.tick > high) {
      continue;
    }
    window.push_back(&sample);
  }
  std::stable_sort(window.begin(),
                   window.end(),
                   [](const MovementTroopSample* lhs, const MovementTroopSample* rhs) {
                     return lhs->tick < rhs->tick;
                   });

  std::ostringstream out;
  out << "tick    state           root(x,z)         yaw     accepted(v)   "
         "advance  remaining  wp     mode        files  corridor/needed  "
         "spacing  gait\n";
  for (auto const* sample : window) {
    out << text("%-7llu %-15s (%7.2f,%7.2f) %7.1f (%6.2f,%6.2f) %8.3f %10.2f "
                "%2u/%-2u %-11s %2u/%-2u %6.2f/%-6.2f %6.2f  %u\n",
                static_cast<unsigned long long>(sample->tick),
                movement_state_name(sample->state),
                static_cast<double>(sample->root_x),
                static_cast<double>(sample->root_z),
                static_cast<double>(sample->root_yaw),
                static_cast<double>(sample->accepted_vx),
                static_cast<double>(sample->accepted_vz),
                static_cast<double>(sample->route_advance),
                static_cast<double>(sample->remaining_arclength),
                sample->waypoint_index,
                sample->waypoint_count,
                traversal_layout_mode_name(sample->traversal_mode),
                sample->current_files,
                sample->normal_files,
                static_cast<double>(sample->corridor_half_width),
                static_cast<double>(sample->formation_half_width),
                static_cast<double>(sample->file_spacing),
                static_cast<unsigned>(sample->presentation_state));
  }
  return out.str();
}

auto format_soldier_timeline(const std::vector<MovementSoldierSample>& soldiers,
                             EntityID troop_id,
                             std::uint32_t slot,
                             std::uint64_t centre_frame,
                             std::uint32_t frames_before,
                             std::uint32_t frames_after) -> std::string {
  std::uint64_t const low =
      centre_frame > frames_before ? centre_frame - frames_before : 0U;
  std::uint64_t const high = centre_frame + frames_after;

  std::vector<const MovementSoldierSample*> window;
  for (auto const& sample : soldiers) {
    if (sample.troop_id != troop_id || sample.stable_slot != slot ||
        sample.frame < low || sample.frame > high) {
      continue;
    }
    window.push_back(&sample);
  }
  std::stable_sort(
      window.begin(),
      window.end(),
      [](const MovementSoldierSample* lhs, const MovementSoldierSample* rhs) {
        return lhs->frame < rhs->frame;
      });

  std::ostringstream out;
  out << "frame   alpha   body(x,z)             ring(x,z)             error     "
         "mode         gait\n";
  for (auto const* sample : window) {
    float const error = std::max(std::fabs(sample->ring_root_x - sample->body_root_x),
                                 std::fabs(sample->ring_root_z - sample->body_root_z));
    out << text("%-7llu %6.3f (%9.4f,%9.4f) (%9.4f,%9.4f) %9.6f %-12s %5.2f\n",
                static_cast<unsigned long long>(sample->frame),
                static_cast<double>(sample->interpolation_alpha),
                static_cast<double>(sample->body_root_x),
                static_cast<double>(sample->body_root_z),
                static_cast<double>(sample->ring_root_x),
                static_cast<double>(sample->ring_root_z),
                static_cast<double>(error),
                traversal_layout_mode_name(sample->traversal_mode),
                static_cast<double>(sample->gait_speed));
  }
  return out.str();
}

auto movement_digest(const std::vector<MovementTroopSample>& troops,
                     const std::vector<MovementSoldierSample>& soldiers)
    -> std::string {

  std::uint64_t hash = 1469598103934665603ULL;
  auto mix = [&hash](std::uint64_t value) {
    for (int byte = 0; byte < 8; ++byte) {
      hash ^= (value >> (byte * 8)) & 0xFFULL;
      hash *= 1099511628211ULL;
    }
  };
  auto mix_position = [&mix](float value) {
    mix(static_cast<std::uint64_t>(
        static_cast<std::int64_t>(std::llround(value * 1000.0F))));
  };

  std::vector<const MovementTroopSample*> ordered;
  ordered.reserve(troops.size());
  for (auto const& sample : troops) {
    ordered.push_back(&sample);
  }
  std::stable_sort(ordered.begin(),
                   ordered.end(),
                   [](const MovementTroopSample* lhs, const MovementTroopSample* rhs) {
                     if (lhs->tick != rhs->tick) {
                       return lhs->tick < rhs->tick;
                     }
                     return lhs->entity_id < rhs->entity_id;
                   });

  for (auto const* sample : ordered) {
    mix(sample->tick);
    mix(sample->entity_id);
    mix(static_cast<std::uint64_t>(sample->state));
    mix(sample->command_sequence);
    mix(sample->route_id);
    mix(sample->route_revision);
    mix_position(sample->lane_offset);
    mix_position(sample->lane_scale);
    mix_position(sample->cohesion_pace);
    mix(sample->portal_id);
    mix(static_cast<std::uint64_t>(sample->traversal_mode));
    mix(sample->current_files);
    mix_position(sample->root_x);
    mix_position(sample->root_z);
  }

  std::vector<const MovementSoldierSample*> ordered_soldiers;
  ordered_soldiers.reserve(soldiers.size());
  for (auto const& sample : soldiers) {
    ordered_soldiers.push_back(&sample);
  }
  std::stable_sort(
      ordered_soldiers.begin(),
      ordered_soldiers.end(),
      [](const MovementSoldierSample* lhs, const MovementSoldierSample* rhs) {
        if (lhs->frame != rhs->frame) {
          return lhs->frame < rhs->frame;
        }
        if (lhs->troop_id != rhs->troop_id) {
          return lhs->troop_id < rhs->troop_id;
        }
        return lhs->stable_slot < rhs->stable_slot;
      });
  for (auto const* sample : ordered_soldiers) {
    mix(sample->troop_id);
    mix(sample->stable_slot);
    mix(static_cast<std::uint64_t>(sample->traversal_mode));
  }

  std::array<char, 24> buffer{};
  std::snprintf(
      buffer.data(), buffer.size(), "%016llx", static_cast<unsigned long long>(hash));
  return std::string(buffer.data());
}

auto load_movement_trace_directory(const std::string& directory,
                                   std::vector<MovementTroopSample>& troops,
                                   std::vector<MovementSoldierSample>& soldiers)
    -> bool {
  std::ifstream troop_stream(directory + "/troops.jsonl");
  if (!troop_stream.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(troop_stream, line)) {
    if (line.empty()) {
      continue;
    }
    MovementTroopSample sample;
    if (parse_troop_sample(line, sample)) {
      troops.push_back(sample);
    }
  }

  std::ifstream soldier_stream(directory + "/soldiers.jsonl");
  if (soldier_stream.is_open()) {
    while (std::getline(soldier_stream, line)) {
      if (line.empty()) {
        continue;
      }
      MovementSoldierSample sample;
      if (parse_soldier_sample(line, sample)) {
        soldiers.push_back(sample);
      }
    }
  }
  return true;
}

} // namespace Engine::Core
