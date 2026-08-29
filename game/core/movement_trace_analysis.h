#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "movement_trace.h"

namespace Engine::Core {

struct MovementGateThresholds {
  float fixed_step_seconds{1.0F / 60.0F};

  float progress_stall_window_seconds{0.35F};
  float progress_stall_advance_metres{0.03F};

  float launch_grace_seconds{0.60F};

  float max_turning_seconds{2.0F};

  float max_recovering_seconds{2.0F};

  float obstruction_response_seconds{0.50F};

  float route_regression_metres{0.50F};
  float route_regression_seconds{0.50F};

  float arrival_settle_speed{0.03F};
  float arrival_settle_seconds{0.20F};

  float heading_flip_degrees{3.0F};
  int heading_flip_run_length{3};

  float reversal_degrees{15.0F};
  float reversal_window_seconds{0.50F};
  int reversal_allowance{1};

  float reversal_min_speed{0.35F};

  float max_angular_speed_degrees{760.0F};
  float max_angular_acceleration_degrees{24000.0F};

  float gait_mismatch_seconds{0.20F};
  float gait_moving_speed{0.20F};
  float gait_stopped_speed{0.03F};

  float collision_recovery_seconds{0.50F};

  float body_overlap_metres{0.15F};
  float body_overlap_seconds{0.50F};

  int layout_toggle_allowance{2};
  float layout_min_dwell_seconds{0.35F};

  float soldier_anchor_jump_metres{0.35F};

  float marker_anchor_tolerance{1.0e-4F};

  float starvation_seconds{90.0F};

  int blocked_step_streak{12};

  bool require_terminal_outcomes{true};

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
  BodyOverlap,
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

[[nodiscard]] auto analyze_active_movement_trace(
    const MovementGateThresholds& thresholds) -> MovementAnalysis;

[[nodiscard]] auto
format_movement_summary(const MovementAnalysis& analysis) -> std::string;

[[nodiscard]] auto
format_movement_findings(const MovementAnalysis& analysis) -> std::string;

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

[[nodiscard]] auto
movement_digest(const std::vector<MovementTroopSample>& troops,
                const std::vector<MovementSoldierSample>& soldiers) -> std::string;

[[nodiscard]] auto
load_movement_trace_directory(const std::string& directory,
                              std::vector<MovementTroopSample>& troops,
                              std::vector<MovementSoldierSample>& soldiers) -> bool;

} // namespace Engine::Core
