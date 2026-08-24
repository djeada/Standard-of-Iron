#pragma once

#include <cstdint>

#include "entity_id.h"

namespace Engine::Core {

// One declared movement state. An accepted order is always in exactly one of
// these, and the terminal ones are the only legal ways for it to stop being
// active: clearing `has_target` without a reason is not an outcome contract.
enum class MovementOrderState : std::uint8_t {
  Idle = 0,
  Following,
  Turning,
  LocallyBlocked,
  Yielding,
  Repathing,
  Recovering,
  Arrived,
  Unreachable,
  Cancelled,
  Superseded
};

[[nodiscard]] auto
is_terminal_movement_state(MovementOrderState state) noexcept -> bool;
[[nodiscard]] auto is_active_movement_state(MovementOrderState state) noexcept -> bool;
[[nodiscard]] auto movement_state_name(MovementOrderState state) noexcept -> const
    char*;

// Where the presented facing came from this tick, so a gait or heading defect
// can be attributed rather than guessed at.
enum class MovementDirectionSource : std::uint8_t {
  None = 0,
  AcceptedVelocity,
  RouteTangent,
  BodyForward,
  DesiredVelocity,
  LayoutRelocation
};

[[nodiscard]] auto
movement_direction_source_name(MovementDirectionSource source) noexcept -> const char*;

// The width ladder a troop's soldiers walk down when a corridor narrows.
enum class TraversalLayoutMode : std::uint8_t {
  Normal = 0,
  NarrowRanks,
  MarchingOrder,
  SingleFile
};

[[nodiscard]] auto
traversal_layout_mode_name(TraversalLayoutMode mode) noexcept -> const char*;

enum class MovementRepathReason : std::uint8_t {
  None = 0,
  GoalChanged,
  TopologyChanged,
  RouteInvalid,
  Blocked,
  ClearanceChanged,
  ObstructionReleased,
  RecoveryEscalation
};

[[nodiscard]] auto
movement_repath_reason_name(MovementRepathReason reason) noexcept -> const char*;

// ---------------------------------------------------------------------------
// The movement facts. Each block has exactly one writing stage; the stage that
// owns it is named in the comment. Nothing downstream may recompute a fact that
// an earlier stage already published.
// ---------------------------------------------------------------------------

// Written by the route follower at the top of the Movement phase, before any
// gate can return early. The trace and the presentation interpolator both need
// "where the body was when this tick started" without depending on whether
// presentation is enabled.
struct RootPoseFacts {
  bool valid{false};
  float x{0.0F};
  float z{0.0F};
  float yaw{0.0F};
};

// Written by the order pipeline (command service / movement orders).
struct RouteIntentFacts {
  std::uint64_t command_sequence{0};
  std::uint64_t route_id{0};
  std::uint64_t route_revision{0};
  std::uint64_t topology_revision{0};
  float requested_goal_x{0.0F};
  float requested_goal_z{0.0F};
  float resolved_goal_x{0.0F};
  float resolved_goal_z{0.0F};
  bool has_goal{false};
};

// Written by the route follower. Immutable input to steering.
struct DesiredMotionFacts {
  bool valid{false};
  float velocity_x{0.0F};
  float velocity_z{0.0F};
  float tangent_x{0.0F};
  float tangent_z{1.0F};
  float lookahead_x{0.0F};
  float lookahead_z{0.0F};
  float speed_limit{0.0F};
  bool turning_in_place{false};
};

enum class SteeringResult : std::uint8_t {
  Unconstrained = 0,
  Deviated,
  Slowed,
  Yielded,
  Separating
};

// Written by the steering stage (local avoidance / queue policy).
struct SteeringFacts {
  bool valid{false};
  float velocity_x{0.0F};
  float velocity_z{0.0F};
  float correction_x{0.0F};
  float correction_z{0.0F};
  // A bounded push out of an overlap that already exists. Kept apart from the
  // steered velocity so ordinary avoidance is never confused with recovering
  // from a bad initial condition, and applied by the motor so it cannot move a
  // root through a wall.
  float separation_x{0.0F};
  float separation_z{0.0F};
  std::uint32_t neighbor_count{0};
  float nearest_time_to_collision{-1.0F};
  std::int8_t passing_side{0};
  SteeringResult result{SteeringResult::Unconstrained};
  EntityID queue_owner{0};
};

// Written by the motor. The only source of truth for "the body moved".
struct MotorFacts {
  bool valid{false};
  float accepted_dx{0.0F};
  float accepted_dz{0.0F};
  float accepted_vx{0.0F};
  float accepted_vz{0.0F};
  float rejected_dx{0.0F};
  float rejected_dz{0.0F};
  float accepted_fraction{1.0F};
  bool blocked{false};
  bool has_contact{false};
  float contact_nx{0.0F};
  float contact_nz{0.0F};
  float penetration_depth{0.0F};
};

// Written by the progress stage.
struct MovementProgressFacts {
  MovementOrderState state{MovementOrderState::Idle};
  MovementOrderState previous_state{MovementOrderState::Idle};
  float remaining_arclength{0.0F};
  float route_advance{0.0F};
  float lateral_route_error{0.0F};
  float no_progress_seconds{0.0F};
  float no_progress_advance{0.0F};
  // Seconds since this order was accepted. The motor's first-order lag means a
  // body needs a bounded launch before it can be judged for lack of progress.
  float order_seconds{0.0F};
  std::uint64_t tracked_order{0};
  float state_seconds{0.0F};
  std::uint32_t blocked_steps{0};
  std::uint32_t repath_count{0};
  std::uint32_t repath_attempts{0};
  MovementRepathReason repath_reason{MovementRepathReason::None};
};

// Which way this body has committed to pass its current encounters, and for how
// long. Persisting the choice is what stops a symmetric pair from swapping
// sides every time the geometry crosses a tie.
struct PassingCommitmentFacts {
  std::int8_t side{0};
  float held_seconds{0.0F};
  // The deviation the solver settled on last tick, in degrees off the route's
  // desired direction. Re-deciding the whole fan from scratch every tick is
  // what makes a body in traffic chatter between two nearly equal answers, so
  // the new answer is rate-limited against this one.
  std::int8_t angle_index{-1};
  float deviation_degrees{0.0F};
};

// Written by the traversal-layout owner.
struct TraversalLayoutFacts {
  TraversalLayoutMode mode{TraversalLayoutMode::Normal};
  TraversalLayoutMode target_mode{TraversalLayoutMode::Normal};
  std::uint32_t portal_id{0};
  std::uint32_t current_files{0};
  std::uint32_t target_files{0};
  float corridor_half_width{0.0F};
  float desired_half_width{0.0F};
  float soldier_body_radius{0.0F};
  float transition_progress{1.0F};
  float mode_dwell_seconds{0.0F};
};

} // namespace Engine::Core
