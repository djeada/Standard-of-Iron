#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "movement_trace.h"

namespace Engine::Core {

// Failure thresholds from the recovery plan's gate table. These are the points
// at which a run is declared broken, not tuning targets: changing one is a
// deliberate act that belongs with an artifact and a rationale.
struct MovementGateThresholds {
  float fixed_step_seconds{1.0F / 60.0F};

  // "Active order with no declared queue/block": no interval over 0.35 s with
  // route progress below 0.03 m.
  float progress_stall_window_seconds{0.35F};
  float progress_stall_advance_metres{0.03F};
  // An order is not judged for lack of progress until the body has had this
  // long to accelerate out of a standstill. See k_launch_grace_seconds.
  float launch_grace_seconds{0.60F};
  // Turning is a declared state, but it may not last forever.
  float max_turning_seconds{2.0F};
  // Nor may recovery: the ladder must reach a terminal outcome.
  float max_recovering_seconds{2.0F};

  // "Persistent obstruction": a repath/queue/unreachable transition must begin
  // within 0.50 s.
  float obstruction_response_seconds{0.50F};

  // "Route progress": remaining arclength may not regress over 0.50 m for more
  // than 0.50 s outside Yielding/Repathing.
  float route_regression_metres{0.50F};
  float route_regression_seconds{0.50F};

  // "Final arrival": velocity settles below 0.03 m/s and locomotion becomes
  // idle within 0.20 s.
  float arrival_settle_speed{0.03F};
  float arrival_settle_seconds{0.20F};

  // "Straight-route heading": no alternating sign changes over 3 degrees on
  // consecutive simulation ticks.
  float heading_flip_degrees{3.0F};
  int heading_flip_run_length{3};

  // "Major direction reversal": at most one uncommanded reversal over
  // 15 degrees in any 0.50 s window.
  float reversal_degrees{15.0F};
  float reversal_window_seconds{0.50F};
  int reversal_allowance{1};
  // Below this the heading of the accepted velocity is numerical noise on a
  // body that is barely moving, not a visible reversal. A creeping body is
  // caught by the stall and gait rules instead.
  float reversal_min_speed{0.35F};

  // Angular rate/acceleration ceilings, degrees per second and per second^2.
  float max_angular_speed_degrees{760.0F};
  float max_angular_acceleration_degrees{24000.0F};

  // "Animation truth": no walk/run state after 0.20 s of accepted speed below
  // 0.03 m/s; no idle state after 0.20 s above 0.20 m/s.
  float gait_mismatch_seconds{0.20F};
  float gait_moving_speed{0.20F};
  float gait_stopped_speed{0.03F};

  // Static collision: any positive penetration after the recovery budget.
  float collision_recovery_seconds{0.50F};

  // "Layout mode stability": one enter and one exit per physical passage.
  int layout_toggle_allowance{2};
  float layout_min_dwell_seconds{0.35F};

  // Soldier layout transition: no instantaneous slot jump.
  float soldier_anchor_jump_metres{0.35F};

  // "Selection/hover ring": exact shared anchor plus a diagnostic tolerance.
  float marker_anchor_tolerance{1.0e-4F};

  // Crowd fairness: every non-cancelled member must resolve within the run.
  float starvation_seconds{90.0F};

  // Blocked-step streaks: repeating the same failed step is a defect even when
  // the watchdog eventually fires.
  int blocked_step_streak{12};

  // Some captures are deliberately truncated; gates that run a scenario to
  // completion keep this on so an order left active is a failure.
  bool require_terminal_outcomes{true};

  // Repath and waypoint churn per order.
  int repath_allowance{6};
  int waypoint_regression_allowance{2};
};

enum class MovementFindingKind : std::uint8_t {
  ProgressStall = 0,
  RouteRegression,
  IndefiniteActiveOrder,
  MissingTerminalOutcome,
  ObstructionNotEscalated,
  BlockedStepStreak,
  RepathChurn,
  WaypointRegression,
  HeadingOscillation,
  DirectionReversal,
  AngularSpeedExceeded,
  AngularAccelerationExceeded,
  ArrivalNotSettled,
  ArrivalRestart,
  GaitWithoutMotion,
  IdleWhileMoving,
  DirectionSourceNotAccepted,
  LayoutModeToggle,
  LayoutModeDwellTooShort,
  LayoutAspectRatio,
  SlotIdentityChanged,
  SoldierAnchorJump,
  MarkerAnchorMismatch,
  ShadowAnchorMismatch,
  PickingAnchorMismatch,
  MissingFinalAnchor,
  CollisionPenetration,
  Starvation
};

[[nodiscard]] auto movement_finding_name(MovementFindingKind kind) noexcept -> const
    char*;

struct MovementFinding {
  MovementFindingKind kind{MovementFindingKind::ProgressStall};
  EntityID entity_id{0};
  std::uint32_t slot{0};
  std::uint64_t first_tick{0};
  std::uint64_t last_tick{0};
  float magnitude{0.0F};
  std::string detail;
};

struct MovementEntitySummary {
  EntityID entity_id{0};
  std::uint64_t first_tick{0};
  std::uint64_t last_tick{0};
  std::uint32_t ticks{0};
  float travelled{0.0F};
  float max_stall_seconds{0.0F};
  float max_route_regression{0.0F};
  std::uint32_t repaths{0};
  std::uint32_t blocked_steps{0};
  std::uint32_t heading_flips{0};
  std::uint32_t layout_transitions{0};
  std::uint32_t findings{0};
  MovementOrderState final_state{MovementOrderState::Idle};
  bool reached_terminal{false};
};

struct MovementSoldierSummary {
  EntityID troop_id{0};
  std::uint32_t slot{0};
  std::uint32_t frames{0};
  float max_anchor_jump{0.0F};
  float max_marker_error{0.0F};
  std::uint32_t findings{0};
};

struct MovementAnalysis {
  std::vector<MovementFinding> findings;
  std::vector<MovementEntitySummary> entities;
  std::vector<MovementSoldierSummary> soldiers;
  std::uint64_t first_failing_tick{0};
  std::uint64_t first_failing_frame{0};
  bool has_failure{false};

  [[nodiscard]] auto passed() const -> bool { return !has_failure; }
  [[nodiscard]] auto count(MovementFindingKind kind) const -> std::size_t;
  [[nodiscard]] auto worst_entity() const -> EntityID;
  [[nodiscard]] auto worst_soldier() const -> const MovementSoldierSummary*;
};

[[nodiscard]] auto
analyze_movement_trace(const std::vector<MovementTroopSample>& troops,
                       const std::vector<MovementSoldierSample>& soldiers,
                       const MovementGateThresholds& thresholds) -> MovementAnalysis;

// Analyzes whatever the live in-memory session holds.
[[nodiscard]] auto analyze_active_movement_trace(
    const MovementGateThresholds& thresholds) -> MovementAnalysis;

[[nodiscard]] auto
format_movement_summary(const MovementAnalysis& analysis) -> std::string;

[[nodiscard]] auto
format_movement_findings(const MovementAnalysis& analysis) -> std::string;

// A textual timeline for one entity around a tick, so the first failure can be
// inspected without opening a multi-minute trace.
[[nodiscard]] auto
format_movement_timeline(const std::vector<MovementTroopSample>& troops,
                         EntityID entity_id,
                         std::uint64_t centre_tick,
                         std::uint32_t ticks_before,
                         std::uint32_t ticks_after) -> std::string;

[[nodiscard]] auto
format_soldier_timeline(const std::vector<MovementSoldierSample>& soldiers,
                        EntityID troop_id,
                        std::uint32_t slot,
                        std::uint64_t centre_frame,
                        std::uint32_t frames_before,
                        std::uint32_t frames_after) -> std::string;

// Stable digest over command outcome, accepted root samples, route revisions,
// portal order, traversal modes, and stable slot mapping.
[[nodiscard]] auto
movement_digest(const std::vector<MovementTroopSample>& troops,
                const std::vector<MovementSoldierSample>& soldiers) -> std::string;

[[nodiscard]] auto
load_movement_trace_directory(const std::string& directory,
                              std::vector<MovementTroopSample>& troops,
                              std::vector<MovementSoldierSample>& soldiers) -> bool;

} // namespace Engine::Core
