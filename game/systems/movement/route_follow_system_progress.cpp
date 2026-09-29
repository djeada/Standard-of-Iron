#include "route_follow_system_progress.h"

#include <algorithm>

#include "order_service.h"

namespace Game::Systems::RouteProgress {

namespace {

constexpr float k_progress_window_metres = 0.03F;
constexpr float k_launch_grace_seconds = 0.60F;
constexpr float k_block_declare_seconds = 0.35F;
constexpr float k_repath_settle_seconds = 0.60F;

} // namespace

auto order_goal_of(const Engine::Core::MovementComponent& movement) -> QVector3D {
  if (movement.get_has_requested_goal()) {
    return {movement.get_requested_goal_x(), 0.0F, movement.get_requested_goal_z()};
  }
  return {movement.get_goal_x(), 0.0F, movement.get_goal_y()};
}

void settle_arrival(Engine::Core::Entity& entity,
                    Engine::Core::MovementComponent& movement,
                    Engine::Core::MovementFactsComponent& facts,
                    MovementRoute& route,
                    bool short_of_the_order) {
  movement.stop();
  OrderService::clear_player_order_intent(&entity);
  auto& progress = facts.progress;
  progress.state = Engine::Core::MovementOrderState::Arrived;
  progress.arrived_short = short_of_the_order;
  progress.no_progress_seconds = 0.0F;
  progress.no_progress_advance = 0.0F;
  progress.remaining_arclength = 0.0F;
  route.clear();
}

namespace {

void begin_order_progress(Engine::Core::MovementProgressFacts& progress,
                          const Engine::Core::MovementComponent& movement) {
  if (progress.tracked_order == movement.get_order_sequence()) {
    return;
  }
  progress.tracked_order = movement.get_order_sequence();
  progress.order_seconds = 0.0F;
  progress.no_progress_seconds = 0.0F;
  progress.no_progress_advance = 0.0F;
  progress.repath_attempts = 0;
  progress.repath_count = 0;
  progress.arrived_short = false;
  progress.short_route_replans = 0;
  progress.holding_at_obstruction = false;
}

void advance_state(Engine::Core::MovementProgressFacts& progress,
                   bool yielding_to_traffic) {
  using Engine::Core::MovementOrderState;
  switch (progress.state) {
  case MovementOrderState::Following:
  case MovementOrderState::Turning:
    if (progress.order_seconds > k_launch_grace_seconds &&
        progress.no_progress_seconds > k_block_declare_seconds) {
      progress.state = MovementOrderState::LocallyBlocked;
    }
    break;

  case MovementOrderState::Yielding:
    if (!yielding_to_traffic) {
      progress.state = MovementOrderState::Following;
    }
    break;

  case MovementOrderState::Repathing:
  case MovementOrderState::Recovering:
    if (progress.state_seconds > k_repath_settle_seconds) {
      progress.state = MovementOrderState::LocallyBlocked;
    }
    break;

  case MovementOrderState::LocallyBlocked:
    break;

  default:
    progress.state = MovementOrderState::Following;
    break;
  }
}

} // namespace

auto update_progress(Engine::Core::MovementComponent& movement,
                     Engine::Core::MovementFactsComponent& facts,
                     float remaining,
                     bool route_changed,
                     float delta_time) -> bool {
  using Engine::Core::MovementOrderState;

  auto& progress = facts.progress;
  MovementOrderState const entry_state = progress.state;

  float const previous_remaining = progress.remaining_arclength;
  bool const comparable = !route_changed && previous_remaining > 0.0F;
  float const advance = comparable ? previous_remaining - remaining : 0.0F;
  progress.route_advance = advance;
  progress.remaining_arclength = remaining;

  begin_order_progress(progress, movement);
  progress.order_seconds += delta_time;

  bool const yielding_to_traffic =
      facts.steering.valid &&
      facts.steering.result == Engine::Core::SteeringResult::Yielded &&
      progress.state != MovementOrderState::Recovering;

  progress.no_progress_seconds += delta_time;
  progress.no_progress_advance += std::max(0.0F, advance);
  bool const covered_ground = progress.no_progress_advance >= k_progress_window_metres;
  if (route_changed || covered_ground) {
    progress.no_progress_seconds = 0.0F;
    progress.no_progress_advance = 0.0F;

    if (covered_ground) {
      progress.repath_attempts = 0;
    }
    if (progress.state != MovementOrderState::Yielding) {
      progress.state = MovementOrderState::Following;
    }
  }

  if (yielding_to_traffic && (progress.state == MovementOrderState::Following ||
                              progress.state == MovementOrderState::Turning ||
                              progress.state == MovementOrderState::Yielding)) {
    progress.state = MovementOrderState::Yielding;
  }

  advance_state(progress, yielding_to_traffic);

  progress.previous_state = entry_state;
  progress.state_seconds =
      progress.state == entry_state ? progress.state_seconds + delta_time : 0.0F;

  return movement.get_has_target();
}

} // namespace Game::Systems::RouteProgress
