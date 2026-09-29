#include "route_follow_system_stall.h"

#include <algorithm>

#include "movement_orders_assignment.h"
#include "order_service.h"
#include "route_follow_system_progress.h"
#include "util/planar_math.h"

namespace Game::Systems::ObjectiveStall {

namespace {

constexpr float k_stall_window_seconds = 1.0F;
constexpr float k_stall_progress_fraction = 0.20F;
constexpr float k_stall_progress_floor_metres = 0.10F;
constexpr float k_stall_progress_ceiling_metres = 0.60F;
constexpr float k_stall_replan_seconds = 1.5F;
constexpr float k_stall_sidestep_seconds = 4.0F;
constexpr float k_stall_abandon_seconds = 12.0F;
constexpr float k_stall_objective_change_metres = 1.5F;
constexpr float k_queue_patience_seconds = 6.0F;

void forget_stall_window(Engine::Core::MovementStallFacts& stall) {
  stall.window_valid = false;
  stall.window_seconds = 0.0F;
}

void restart_stall_ladder(Engine::Core::MovementStallFacts& stall) {
  stall.stalled_seconds = 0.0F;
  stall.no_closer_seconds = 0.0F;
  stall.queued_seconds = 0.0F;
  stall.rung = Engine::Core::MovementRecoveryRung::None;
  forget_stall_window(stall);
}

[[nodiscard]] auto ladder_rung(float seconds,
                               float replan_at,
                               float sidestep_at,
                               float abandon_at) -> Engine::Core::MovementRecoveryRung {
  using Rung = Engine::Core::MovementRecoveryRung;
  if (seconds >= abandon_at) {
    return Rung::Abandoned;
  }
  if (seconds >= sidestep_at) {
    return Rung::Sidestep;
  }
  if (seconds >= replan_at) {
    return Rung::Replan;
  }
  return Rung::None;
}

[[nodiscard]] auto rung_for(const Engine::Core::MovementStallFacts& stall)
    -> Engine::Core::MovementRecoveryRung {
  return ladder_rung(stall.no_closer_seconds,
                     k_stall_replan_seconds,
                     k_stall_sidestep_seconds,
                     k_stall_abandon_seconds);
}

} // namespace

void abandon(Engine::Core::Entity& entity,
             Engine::Core::MovementComponent& movement,
             Engine::Core::MovementFactsComponent& facts,
             const QVector3D& objective) {
  movement.stop();
  OrderService::clear_player_order_intent(&entity);

  auto& progress = facts.progress;
  progress.state = Engine::Core::MovementOrderState::Unreachable;

  progress.holding_at_obstruction = false;
  progress.no_progress_seconds = 0.0F;
  progress.no_progress_advance = 0.0F;
  progress.remaining_arclength = 0.0F;

  auto& stall = progress.stall;
  if (!stall.objective_abandoned) {
    ++stall.abandon_count;
  }
  stall.objective_abandoned = true;
  stall.abandoned_x = objective.x();
  stall.abandoned_z = objective.z();
  stall.rung = Engine::Core::MovementRecoveryRung::Abandoned;
  forget_stall_window(stall);
}

namespace {

void note_objective(Engine::Core::MovementStallFacts& stall,
                    const Engine::Core::MovementComponent& movement,
                    const QVector3D& objective) {
  bool const objective_moved =
      !stall.objective_valid ||
      Game::Systems::planar_length(objective.x() - stall.objective_x,
                                   objective.z() - stall.objective_z) >
          k_stall_objective_change_metres;

  if (objective_moved) {
    stall.objective_valid = true;
    stall.objective_x = objective.x();
    stall.objective_z = objective.z();
    stall.recovery_attempts = 0;
    stall.abandon_count = 0;
    stall.objective_abandoned = false;
    restart_stall_ladder(stall);
  }

  if (stall.tracked_order != movement.get_order_sequence()) {
    stall.tracked_order = movement.get_order_sequence();
    stall.objective_abandoned = false;
    restart_stall_ladder(stall);
  }
}

auto advance_stall_clocks(Engine::Core::MovementFactsComponent& facts,
                          const QVector3D& here,
                          float max_speed,
                          float delta_time) -> bool {
  auto& stall = facts.progress.stall;

  if (facts.progress.route_advance >=
      max_speed * delta_time * k_stall_progress_fraction) {
    stall.no_closer_seconds = 0.0F;
  } else {
    stall.no_closer_seconds += delta_time;
  }

  if (!stall.window_valid) {
    stall.window_valid = true;
    stall.window_seconds = 0.0F;
    stall.window_reference_x = here.x();
    stall.window_reference_z = here.z();
  }

  bool const queued_behind_traffic =
      facts.steering.valid &&
      facts.steering.result == Engine::Core::SteeringResult::Yielded;

  if (queued_behind_traffic) {
    stall.queued_seconds += delta_time;
  } else {
    stall.queued_seconds = 0.0F;
  }

  stall.window_seconds += delta_time;
  if (stall.window_seconds >= k_stall_window_seconds) {
    float const moved = Game::Systems::planar_length(
        here.x() - stall.window_reference_x, here.z() - stall.window_reference_z);

    float const expected =
        queued_behind_traffic
            ? k_stall_progress_floor_metres
            : std::clamp(max_speed * stall.window_seconds * k_stall_progress_fraction,
                         k_stall_progress_floor_metres,
                         k_stall_progress_ceiling_metres);
    float const window = stall.window_seconds;
    stall.window_seconds = 0.0F;
    stall.window_reference_x = here.x();
    stall.window_reference_z = here.z();
    if (moved >= expected) {
      stall.stalled_seconds = 0.0F;
    } else {
      stall.stalled_seconds += window;
    }
  }
  return queued_behind_traffic;
}

auto apply_rung(FollowFrame& frame,
                Engine::Core::MovementRecoveryRung wanted,
                const QVector3D& objective,
                const QVector3D& here,
                bool queued_behind_traffic) -> bool {
  using Engine::Core::MovementOrderState;
  using Engine::Core::MovementRecoveryRung;
  using Engine::Core::MovementRepathReason;
  auto& facts = frame.facts;
  auto& stall = facts.progress.stall;

  switch (wanted) {
  case MovementRecoveryRung::None:
    break;

  case MovementRecoveryRung::Replan:
    MovementSystem::Assignment::retarget_unit(
        frame.world, frame.entity.get_id(), objective);
    ++facts.progress.repath_count;
    facts.progress.repath_reason = MovementRepathReason::Blocked;
    facts.progress.state = MovementOrderState::Repathing;
    break;

  case MovementRecoveryRung::Sidestep:
    if (queued_behind_traffic) {
      MovementSystem::Assignment::retarget_unit(
          frame.world, frame.entity.get_id(), objective);
      ++facts.progress.repath_count;
      facts.progress.repath_reason = MovementRepathReason::Blocked;
      break;
    }
    if (MovementSystem::Assignment::assign_local_recovery_move(
            here, QVector3D(objective.x(), 0.0F, objective.z()), &frame.movement)) {
      facts.progress.state = MovementOrderState::Recovering;
      facts.progress.repath_reason = MovementRepathReason::RecoveryEscalation;
    } else {
      MovementSystem::Assignment::retarget_unit(
          frame.world, frame.entity.get_id(), objective);
      ++facts.progress.repath_count;
      facts.progress.repath_reason = MovementRepathReason::RecoveryEscalation;
    }
    break;

  case MovementRecoveryRung::Abandoned:
    if (queued_behind_traffic) {
      stall.stalled_seconds = 0.0F;
      stall.rung = MovementRecoveryRung::None;
      forget_stall_window(stall);
      break;
    }
    abandon(frame.entity, frame.movement, facts, objective);
    return true;
  }
  return false;
}

} // namespace

auto track(FollowFrame& frame, float max_speed) -> bool {
  using Engine::Core::MovementRecoveryRung;

  auto& movement = frame.movement;
  auto& facts = frame.facts;
  auto& stall = facts.progress.stall;

  if (!movement.get_has_target()) {
    stall.objective_valid = false;
    stall.stalled_seconds = 0.0F;
    stall.no_closer_seconds = 0.0F;
    if (!stall.objective_abandoned) {
      stall.rung = MovementRecoveryRung::None;
    }
    forget_stall_window(stall);
    return false;
  }

  QVector3D const objective = RouteProgress::order_goal_of(movement);
  QVector3D const here(frame.transform.position.x, 0.0F, frame.transform.position.z);

  note_objective(stall, movement, objective);
  bool const queued_behind_traffic =
      advance_stall_clocks(facts, here, max_speed, frame.delta_time);

  if (queued_behind_traffic && stall.queued_seconds < k_queue_patience_seconds) {
    return false;
  }

  auto const wanted = rung_for(stall);

  if (facts.progress.holding_at_obstruction &&
      wanted != MovementRecoveryRung::Abandoned) {
    return false;
  }
  if (wanted < stall.rung) {
    stall.rung = wanted;
    return false;
  }
  if (wanted == stall.rung) {
    return false;
  }
  stall.rung = wanted;

  stall.recovery_attempts += wanted != MovementRecoveryRung::Abandoned;

  return apply_rung(frame, wanted, objective, here, queued_behind_traffic);
}

} // namespace Game::Systems::ObjectiveStall
