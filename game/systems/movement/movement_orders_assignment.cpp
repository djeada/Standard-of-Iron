#include "movement_orders_assignment.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "command_service.h"
#include "formation/army_formation_registry.h"
#include "movement_orders_targets.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/route_corridor_planner.h"

namespace Game::Systems {

using namespace MovementTargeting;

namespace {

constexpr float same_target_threshold_sq = 0.01F;
constexpr float k_route_keep_goal_shift = 1.5F;

} // namespace

void MovementSystem::Assignment::stamp_route_revision(
    Engine::Core::MovementComponent& movement) {
  auto const* pathfinder = NavGrid::get_pathfinder();
  movement.begin_route(pathfinder != nullptr ? pathfinder->navigation_revision() : 0U);
}

void MovementSystem::Assignment::assign_direct_target(
    Engine::Core::MovementComponent& movement, const QVector3D& target) {
  stamp_route_revision(movement);
  if (movement.route_id == 0U) {
    movement.route_id =
        RouteCorridorPlanner::identity({target}, movement.get_topology_revision());
  }
  movement.clear_path();
  movement.target_x = target.x();
  movement.target_y = target.z();
  movement.goal_x = target.x();
  movement.goal_y = target.z();
  movement.has_target = true;
}

namespace {

auto plain_waypoints(const std::vector<QVector3D>& waypoints)
    -> std::vector<std::pair<float, float>> {
  std::vector<std::pair<float, float>> plain;
  plain.reserve(waypoints.size());
  for (const auto& waypoint : waypoints) {
    if (!plain.empty()) {
      float const dx = waypoint.x() - plain.back().first;
      float const dz = waypoint.z() - plain.back().second;
      if ((dx * dx) + (dz * dz) <= 1.0e-6F) {
        continue;
      }
    }
    plain.emplace_back(waypoint.x(), waypoint.z());
  }
  return plain;
}

} // namespace

void MovementSystem::Assignment::append_aligned_waypoints(
    const std::vector<QVector3D>& waypoints,
    Engine::Core::MovementComponent& movement) {
  for (std::size_t index = 0; index < waypoints.size(); ++index) {
    QVector3D const waypoint = align_portal_waypoint(
        waypoints[index],
        index + 1U == waypoints.size(),
        movement.path.empty()
            ? std::optional<QVector3D>{}
            : std::optional<QVector3D>{QVector3D(
                  movement.path.back().first, 0.0F, movement.path.back().second)});
    if (!movement.path.empty()) {
      auto const& previous = movement.path.back();
      float const dx = waypoint.x() - previous.first;
      float const dz = waypoint.z() - previous.second;
      if (dx * dx + dz * dz <= 1.0e-6F) {
        continue;
      }
    }
    movement.path.emplace_back(waypoint.x(), waypoint.z());
  }
}

void MovementSystem::Assignment::straighten_path(
    Pathfinding& pathfinder,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement) {
  auto const cornered = movement.path;
  pull_path_taut(pathfinder,
                 transform,
                 passability_for(movement),
                 Pathfinding::routing_clearance(movement.get_navigation_clearance()),
                 movement.path);
  if (!path_legs_are_walkable(
          pathfinder, transform, passability_for(movement), movement.path)) {
    movement.path = cornered;
  }
}

void MovementSystem::Assignment::skip_reached_waypoints(
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement) {
  constexpr float skip_threshold_sq = CommandService::WAYPOINT_SKIP_THRESHOLD_SQ;
  while (movement.has_waypoints()) {
    const auto& wp = movement.current_waypoint();
    float const dx = wp.first - transform.position.x;
    float const dz = wp.second - transform.position.z;
    if (dx * dx + dz * dz <= skip_threshold_sq) {
      movement.advance_waypoint();
    } else {
      break;
    }
  }
}

auto MovementSystem::Assignment::assign_waypoints_to_movement(
    Pathfinding& pathfinder,
    const std::vector<QVector3D>& waypoints,
    const QVector3D& resolved_goal,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement) -> bool {
  if (waypoints.empty()) {
    return false;
  }

  stamp_route_revision(movement);
  if (movement.route_id == 0U) {
    movement.route_id =
        RouteCorridorPlanner::identity(waypoints, movement.get_topology_revision());
  }
  movement.clear_path();
  movement.route_lane_min_scale = 1.0F;
  movement.route_opening_waypoint_index = 0U;
  movement.route_reform_waypoint_index = 0U;
  movement.path.reserve(waypoints.size());

  std::vector<std::pair<float, float>> plain = plain_waypoints(waypoints);
  append_aligned_waypoints(waypoints, movement);

  if (!path_legs_are_walkable(
          pathfinder, transform, passability_for(movement), movement.path)) {
    movement.path = plain;
  }
  straighten_path(pathfinder, transform, movement);
  skip_reached_waypoints(transform, movement);

  if (!movement.has_waypoints()) {
    return false;
  }

  const auto& wp = movement.current_waypoint();
  movement.target_x = wp.first;
  movement.target_y = wp.second;
  movement.goal_x = resolved_goal.x();
  movement.goal_y = resolved_goal.z();
  movement.requested_goal_x = resolved_goal.x();
  movement.requested_goal_z = resolved_goal.z();
  movement.has_requested_goal = true;
  movement.has_target = true;
  return true;
}

auto MovementSystem::Assignment::keep_route_toward(
    Pathfinding* pathfinder,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement,
    const QVector3D& requested_target) -> bool {
  if (!(movement.has_requested_goal && movement.has_target &&
        movement.has_waypoints() && movement.remaining_waypoints() > 1U)) {
    return false;
  }
  float const moved_x = requested_target.x() - movement.requested_goal_x;
  float const moved_z = requested_target.z() - movement.requested_goal_z;

  bool old_route_still_leads_there = true;
  if (movement.has_waypoints()) {
    auto const& next_waypoint = movement.path[movement.path_index];
    float const to_waypoint_x = next_waypoint.first - transform.position.x;
    float const to_waypoint_z = next_waypoint.second - transform.position.z;
    float const to_goal_x = requested_target.x() - transform.position.x;
    float const to_goal_z = requested_target.z() - transform.position.z;
    old_route_still_leads_there =
        (to_waypoint_x * to_goal_x) + (to_waypoint_z * to_goal_z) >= 0.0F;
  }
  if (old_route_still_leads_there &&
      moved_x * moved_x + moved_z * moved_z <=
          k_route_keep_goal_shift * k_route_keep_goal_shift &&
      (pathfinder == nullptr ||
       pathfinder->is_world_position_walkable(
           requested_target, passability_for(movement), 0.0F))) {
    movement.requested_goal_x = requested_target.x();
    movement.requested_goal_z = requested_target.z();
    movement.path.back() = {requested_target.x(), requested_target.z()};
    movement.goal_x = requested_target.x();
    movement.goal_y = requested_target.z();
    return true;
  }
  return false;
}

void MovementSystem::Assignment::route_to_planned_target(
    Pathfinding& pathfinder,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement,
    const QVector3D& current_pos,
    const QVector3D& planned_target) {
  Point const start =
      NavGrid::world_to_grid(transform.position.x, transform.position.z);
  Point const end = NavGrid::world_to_grid(planned_target.x(), planned_target.z());

  bool const portal_route =
      segment_traverses_navigation_portal(current_pos, planned_target);
  bool const direct_clear =
      is_direct_path_walkable(current_pos,
                              planned_target,
                              passability_for(movement),
                              movement.get_navigation_clearance());
  if ((start == end && direct_clear) || (direct_clear && !portal_route)) {
    assign_direct_target(
        movement,
        resolve_walkable_direct_target(planned_target, passability_for(movement)));
    return;
  }

  auto const corridor = RouteCorridorPlanner::plan(pathfinder,
                                                   current_pos,
                                                   planned_target,
                                                   passability_for(movement),
                                                   movement.get_navigation_clearance());
  if (!corridor.reachable() || !assign_waypoints_to_movement(pathfinder,
                                                             corridor.centerline,
                                                             corridor.centerline.back(),
                                                             transform,
                                                             movement)) {
    QVector3D const fallback =
        corridor.centerline.empty()
            ? resolve_walkable_direct_target(planned_target, passability_for(movement))
            : corridor.centerline.back();
    assign_direct_target(movement, fallback);
  }
}

void MovementSystem::Assignment::assign_navigation_target(
    Pathfinding* pathfinder,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement,
    const QVector3D& requested_target) {
  if (keep_route_toward(pathfinder, transform, movement, requested_target)) {
    return;
  }
  movement.requested_goal_x = requested_target.x();
  movement.requested_goal_z = requested_target.z();
  movement.has_requested_goal = true;

  if (pathfinder == nullptr) {
    assign_direct_target(movement, requested_target);
    return;
  }

  QVector3D const current_pos(transform.position.x, 0.0F, transform.position.z);
  QVector3D const planned_target = resolve_walkable_target_toward(
      requested_target, current_pos, passability_for(movement));

  movement.end_escape();
  if (assign_escape_if_sealed(*pathfinder, transform, movement, planned_target)) {
    movement.requested_goal_x = requested_target.x();
    movement.requested_goal_z = requested_target.z();
    movement.has_requested_goal = true;
    return;
  }

  route_to_planned_target(
      *pathfinder, transform, movement, current_pos, planned_target);

  movement.requested_goal_x = requested_target.x();
  movement.requested_goal_z = requested_target.z();
  movement.has_requested_goal = true;
}

auto MovementSystem::Assignment::assign_escape_if_sealed(
    Pathfinding& pathfinder,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement,
    const QVector3D& target) -> bool {
  Point const here = NavGrid::world_to_grid(transform.position.x, transform.position.z);
  if (!pathfinder.is_walkable(here.x, here.y, passability_for(movement))) {
    return false;
  }
  auto const exit_cell = pathfinder.find_escape_point(
      here, NavGrid::world_to_grid(target.x(), target.z()), passability_for(movement));
  if (!exit_cell.has_value()) {
    return false;
  }
  QVector3D const exit = NavGrid::grid_to_world(*exit_cell);
  Engine::Core::TransformComponent at_exit = transform;
  at_exit.position.x = exit.x();
  at_exit.position.z = exit.z();
  auto const corridor = RouteCorridorPlanner::plan(pathfinder,
                                                   QVector3D(exit.x(), 0.0F, exit.z()),
                                                   target,
                                                   passability_for(movement),
                                                   movement.get_navigation_clearance());
  bool const routed =
      corridor.reachable() && assign_waypoints_to_movement(pathfinder,
                                                           corridor.centerline,
                                                           corridor.centerline.back(),
                                                           at_exit,
                                                           movement);
  if (!routed) {
    assign_direct_target(movement, target);
    movement.path.emplace_back(target.x(), target.z());
    movement.path_index = 0;
  }
  movement.path.insert(movement.path.begin() +
                           static_cast<std::ptrdiff_t>(movement.path_index),
                       {exit.x(), exit.z()});
  movement.target_x = exit.x();
  movement.target_y = exit.z();
  movement.has_target = true;
  movement.begin_escape(exit.x(), exit.z());
  return true;
}

auto MovementSystem::Assignment::assign_local_recovery_move(
    const QVector3D& current_position,
    const QVector3D& goal,
    Engine::Core::MovementComponent* movement) -> bool {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr || movement == nullptr) {
    return false;
  }

  Point const current_grid =
      NavGrid::world_to_grid(current_position.x(), current_position.z());

  Point recovery_cell{};
  if (!find_recovery_cell(
          *pathfinder, current_grid, passability_for(*movement), recovery_cell)) {

    constexpr int k_emergency_search_radius = 64;
    auto const nearest =
        NavGrid::find_nearest_walkable_grid(current_grid, k_emergency_search_radius);
    if (!nearest.has_value()) {
      return false;
    }
    recovery_cell = *nearest;
  }

  QVector3D const safe_pos = NavGrid::grid_to_world(recovery_cell);
  bool const had_active_target = movement->has_target;
  float const active_target_dx = safe_pos.x() - movement->target_x;
  float const active_target_dz = safe_pos.z() - movement->target_y;
  if (movement->has_target &&
      active_target_dx * active_target_dx + active_target_dz * active_target_dz <=
          same_target_threshold_sq) {
    return false;
  }

  movement->target_x = safe_pos.x();
  movement->target_y = safe_pos.z();
  std::vector<std::pair<float, float>> recovery_waypoints;
  recovery_waypoints.emplace_back(safe_pos.x(), safe_pos.z());

  QVector3D resolved_goal = safe_pos;
  if (had_active_target) {
    Point const desired_goal = NavGrid::world_to_grid(goal.x(), goal.z());
    auto const route = pathfinder->find_path(recovery_cell,
                                             desired_goal,
                                             passability_for(*movement),
                                             movement->get_navigation_clearance());
    if (route.size() > 1) {
      recovery_waypoints.reserve(route.size());
      for (std::size_t idx = 1; idx < route.size(); ++idx) {
        QVector3D const waypoint = pathfinder->path_waypoint_world_position(route[idx]);
        recovery_waypoints.emplace_back(waypoint.x(), waypoint.z());
      }
      resolved_goal = pathfinder->path_waypoint_world_position(route.back());
    }
  }

  movement->goal_x = resolved_goal.x();
  movement->goal_y = resolved_goal.z();
  movement->has_target = true;
  if (movement->path_index <= movement->path.size()) {
    auto const insert_pos =
        movement->path.begin() + static_cast<std::ptrdiff_t>(movement->path_index);
    movement->path.erase(insert_pos, movement->path.end());
  } else {
    movement->path_index = movement->path.size();
  }
  movement->path.insert(
      movement->path.end(), recovery_waypoints.begin(), recovery_waypoints.end());

  movement->begin_route(pathfinder->navigation_revision());
  return true;
}

void MovementSystem::Assignment::adopt_formation_group(
    Engine::Core::World& world,
    Engine::Core::EntityID unit_id,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement) {
  const auto* membership =
      world.try_get<Engine::Core::ArmyFormationMembershipComponent>(unit_id);
  const auto* group =
      membership != nullptr
          ? Game::Formation::ArmyFormationRegistry::for_world(world).find(
                membership->group_id)
          : nullptr;
  if (group != nullptr) {
    if (group->morph.active) {
      const auto* unit = world.try_get<Engine::Core::UnitComponent>(unit_id);
      movement.declared_group_pace = Game::Formation::ArmyFormationRuntime::morph_pace(
          *group,
          unit_id,
          QVector3D(transform.position.x, 0.0F, transform.position.z),
          unit != nullptr ? unit->speed : 1.0F);
    } else {
      movement.declared_group_pace = group->cohesion_pace;
    }
    movement.route_id = RouteCorridorPlanner::identity(
        group->move_plan.corridor,
        NavGrid::get_pathfinder() != nullptr
            ? NavGrid::get_pathfinder()->navigation_revision()
            : 0U);
    if (const auto* slot = group->find_slot_for(unit_id)) {
      movement.route_lane_offset = slot->local_offset.x();
    }
  }
}

auto MovementSystem::Assignment::retarget_unit(Engine::Core::World& world,
                                               Engine::Core::EntityID entity_id,
                                               const QVector3D& goal) -> bool {
  auto* movement = world.try_get<Engine::Core::MovementComponent>(entity_id);
  auto* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
  if (movement == nullptr || transform == nullptr) {
    return false;
  }

  movement->has_requested_goal = false;
  assign_navigation_target(NavGrid::get_pathfinder(), *transform, *movement, goal);
  return true;
}

} // namespace Game::Systems
