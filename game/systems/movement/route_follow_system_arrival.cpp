#include "route_follow_system_arrival.h"

#include <algorithm>

#include "movement_orders_assignment.h"
#include "route_follow_system_gate.h"
#include "route_follow_system_progress.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "util/planar_math.h"

namespace Game::Systems::RouteArrival {

using RouteProgress::order_goal_of;
using RouteProgress::settle_arrival;

namespace {

constexpr float k_short_route_slack = 0.75F;
constexpr float k_hold_recheck_seconds = 1.0F;
constexpr float k_occupied_goal_slack = 1.5F;
constexpr float k_occupied_goal_press_seconds = 0.5F;

auto route_stops_short_of_the_order(const Engine::Core::MovementComponent& movement,
                                    const Engine::Core::TransformComponent& transform,
                                    float arrive_radius) -> bool {
  if (!movement.get_has_requested_goal()) {
    return false;
  }

  float const to_requested = Game::Systems::planar_length(
      movement.get_requested_goal_x() - transform.position.x,
      movement.get_requested_goal_z() - transform.position.z);
  return to_requested > arrive_radius + k_short_route_slack;
}

auto route_now_reaches(const Engine::Core::MovementComponent& movement,
                       const QVector3D& from,
                       const QVector3D& goal) -> bool {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return false;
  }
  pathfinder->update_navigation_grid();
  Point const start = NavGrid::world_to_grid(from.x(), from.z());
  Point const end = NavGrid::world_to_grid(goal.x(), goal.z());
  auto const passability = movement.get_can_enter_forest()
                               ? Pathfinding::Passability::Light
                               : Pathfinding::Passability::Heavy;
  std::uint32_t const start_region = pathfinder->region_of(start, passability);
  if (start_region != Pathfinding::k_unreachable_region &&
      start_region != pathfinder->region_of(end, passability)) {
    return false;
  }
  auto const path = pathfinder->find_path(
      start, end, passability, movement.get_navigation_clearance());
  return !path.empty() && path.back() == end;
}

void repath_to(FollowFrame& frame,
               const QVector3D& goal,
               Engine::Core::MovementRepathReason reason) {
  MovementSystem::Assignment::retarget_unit(frame.world, frame.entity.get_id(), goal);
  auto& progress = frame.facts.progress;
  ++progress.repath_count;
  progress.repath_reason = reason;
  progress.state = Engine::Core::MovementOrderState::Repathing;
}

void handle_stopped_short(FollowFrame& frame, MovementRoute& route) {
  auto& movement = frame.movement;
  auto& facts = frame.facts;
  auto& transform = frame.transform;

  QVector3D const requested = order_goal_of(movement);
  bool const requested_point_is_ground =
      is_movement_point_allowed(requested, frame.entity);
  if (!requested_point_is_ground) {
    settle_arrival(frame.entity, movement, facts, route, true);
    return;
  }
  if (facts.progress.short_route_replans == 0U) {
    ++facts.progress.short_route_replans;
    repath_to(frame, requested, Engine::Core::MovementRepathReason::RouteInvalid);
    return;
  }

  facts.progress.holding_at_obstruction = true;
  facts.progress.holding_recheck_seconds += frame.delta_time;
  if (facts.progress.holding_recheck_seconds >= k_hold_recheck_seconds) {
    facts.progress.holding_recheck_seconds = 0.0F;
    QVector3D const here(transform.position.x, 0.0F, transform.position.z);
    if (route_now_reaches(movement, here, requested)) {
      facts.progress.holding_at_obstruction = false;
      facts.progress.short_route_replans = 0;
      repath_to(
          frame, requested, Engine::Core::MovementRepathReason::ObstructionReleased);
      return;
    }
  }
  facts.progress.state = Engine::Core::MovementOrderState::LocallyBlocked;
}

} // namespace

auto resolve(FollowFrame& frame,
             MovementRoute& route,
             const RouteFollowSystem::Steering::RouteAim& aim,
             float arrive_radius,
             bool current_position_allowed) -> bool {
  auto& movement = frame.movement;
  auto& facts = frame.facts;
  auto& transform = frame.transform;
  float const remaining = aim.remaining;

  float const endpoint_distance = Game::Systems::planar_length(
      aim.endpoint_x - transform.position.x, aim.endpoint_z - transform.position.z);
  bool const pressed_against_a_standing_friend =
      facts.steering.valid && facts.steering.body_overlap > 0.0F &&
      facts.progress.no_progress_seconds > k_occupied_goal_press_seconds;
  bool const goal_is_held_by_friends =
      current_position_allowed && pressed_against_a_standing_friend &&
      !movement.get_issuer_retargets() &&
      movement.get_structure_approach_target() == 0 &&
      remaining <= arrive_radius + k_occupied_goal_slack &&
      endpoint_distance <= arrive_radius + k_occupied_goal_slack;
  if (goal_is_held_by_friends) {
    settle_arrival(frame.entity, movement, facts, route, true);
    return true;
  }

  if (!(current_position_allowed && remaining <= arrive_radius &&
        endpoint_distance <= arrive_radius)) {
    return false;
  }

  bool const stopped_short =
      route_stops_short_of_the_order(movement, transform, arrive_radius);
  if (stopped_short && (movement.get_issuer_retargets() ||
                        movement.get_structure_approach_target() != 0)) {
    facts.progress.state = Engine::Core::MovementOrderState::LocallyBlocked;
    return true;
  }
  if (stopped_short) {
    handle_stopped_short(frame, route);
    return true;
  }
  settle_arrival(frame.entity, movement, facts, route, false);

  auto* guard_mode =
      frame.world.try_get<Engine::Core::GuardModeComponent>(frame.entity.get_id());
  if ((guard_mode != nullptr) && guard_mode->active &&
      guard_mode->returning_to_guard_position) {
    guard_mode->returning_to_guard_position = false;
  }
  return true;
}

} // namespace Game::Systems::RouteArrival
