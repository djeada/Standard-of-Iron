#pragma once

#include <cstdint>

#include "entity_id.h"

namespace Engine::Core {

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

struct RootPoseFacts {
  bool valid{false};
  float x{0.0F};
  float z{0.0F};
  float yaw{0.0F};
};

struct RouteIntentFacts {
  std::uint64_t command_sequence{0};
  std::uint64_t route_id{0};
  std::uint64_t route_revision{0};
  std::uint64_t topology_revision{0};
  float lane_offset{0.0F};
  float lane_scale{1.0F};
  float cohesion_pace{0.0F};
  float requested_goal_x{0.0F};
  float requested_goal_z{0.0F};
  float resolved_goal_x{0.0F};
  float resolved_goal_z{0.0F};
  bool has_goal{false};
};

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

struct SteeringFacts {
  bool valid{false};
  float velocity_x{0.0F};
  float velocity_z{0.0F};
  float correction_x{0.0F};
  float correction_z{0.0F};

  float separation_x{0.0F};
  float separation_z{0.0F};
  std::uint32_t neighbor_count{0};
  float nearest_time_to_collision{-1.0F};
  std::int8_t passing_side{0};
  SteeringResult result{SteeringResult::Unconstrained};
  EntityID queue_owner{0};
};

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

struct MovementProgressFacts {
  MovementOrderState state{MovementOrderState::Idle};
  MovementOrderState previous_state{MovementOrderState::Idle};
  float remaining_arclength{0.0F};
  float route_advance{0.0F};
  float lateral_route_error{0.0F};
  float no_progress_seconds{0.0F};
  float no_progress_advance{0.0F};

  float order_seconds{0.0F};
  std::uint64_t tracked_order{0};
  float state_seconds{0.0F};
  std::uint32_t blocked_steps{0};
  std::uint32_t repath_count{0};
  std::uint32_t repath_attempts{0};
  MovementRepathReason repath_reason{MovementRepathReason::None};
};

struct PassingCommitmentFacts {
  std::int8_t side{0};
  float held_seconds{0.0F};

  std::int8_t angle_index{-1};
  float deviation_degrees{0.0F};
};

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
