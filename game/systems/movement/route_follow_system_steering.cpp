#include "route_follow_system_steering.h"

#include <algorithm>
#include <cmath>

#include "command_service.h"
#include "formation/army_formation_registry.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "util/planar_math.h"

namespace Game::Systems {

namespace {

constexpr float k_lookahead_speed_seconds = 0.9F;
constexpr float k_lookahead_min = 1.5F;
constexpr float k_lookahead_max = 4.0F;
constexpr float k_projection_window_min = 1.5F;
constexpr float k_degenerate_aim_distance = 0.15F;

constexpr float k_minimum_aim_fraction = 0.75F;

} // namespace

void RouteFollowSystem::Steering::aim_on_route(FollowFrame& frame,
                                               MovementRoute& route,
                                               float max_speed,
                                               RouteAim& result) {
  auto* transform = &frame.transform;
  auto* movement = &frame.movement;
  auto* facts = &frame.facts;
  float const delta_time = frame.delta_time;

  float const window =
      std::max(k_projection_window_min,
               max_speed * delta_time * 8.0F + movement->get_navigation_clearance());
  auto const projection =
      route.project(transform->position.x, transform->position.z, window);
  route.advance_to(projection.s);
  float const s = route.travelled();
  result.remaining = route.remaining();
  facts->progress.lateral_route_error = projection.lateral;

  std::size_t const target_index = route.waypoint_index_at(s);
  while (movement->get_path_index() < target_index && movement->has_waypoints()) {
    movement->advance_waypoint();
  }
  auto const final_point = route.final_point();
  result.endpoint_x = final_point.first;
  result.endpoint_z = final_point.second;
  if (movement->has_waypoints()) {
    auto const& waypoint = movement->current_waypoint();
    movement->target_x = waypoint.first;
    movement->target_y = waypoint.second;
  } else {
    movement->target_x = result.endpoint_x;
    movement->target_y = result.endpoint_z;
  }

  float const lookahead = std::clamp(
      max_speed * k_lookahead_speed_seconds, k_lookahead_min, k_lookahead_max);
  auto const* pathfinder = NavGrid::get_pathfinder();
  auto const aim_passability = movement->get_can_enter_forest()
                                   ? Pathfinding::Passability::Light
                                   : Pathfinding::Passability::Heavy;
  float aim_s = route.steering_aim_s(
      transform->position.x,
      transform->position.z,
      lookahead,
      lookahead * k_minimum_aim_fraction,
      [pathfinder,
       aim_passability](float from_x, float from_z, float to_x, float to_z) {
        return pathfinder == nullptr ||
               pathfinder->is_world_segment_walkable(QVector3D(from_x, 0.0F, from_z),
                                                     QVector3D(to_x, 0.0F, to_z),
                                                     aim_passability,
                                                     0.0F);
      });
  auto aim = route.point_at(aim_s);

  constexpr int k_max_degenerate_advances = 8;
  for (int advance = 0;
       advance < k_max_degenerate_advances &&
       Game::Systems::planar_length(aim.first - transform->position.x,
                                    aim.second - transform->position.z) <
           k_degenerate_aim_distance &&
       aim_s < route.length();
       ++advance) {
    aim_s = std::min(route.length(), route.next_vertex_s(aim_s));
    aim = route.point_at(aim_s);
  }
  result.aim_x = aim.first;
  result.aim_z = aim.second;

  auto const route_origin = route.point_at(s);
  float const chord_x = result.aim_x - route_origin.first;
  float const chord_z = result.aim_z - route_origin.second;
  float const chord_length = std::hypot(chord_x, chord_z);
  auto const tangent = route.tangent_at(s);
  result.tangent_x = chord_length > 0.001F ? chord_x / chord_length : tangent.first;
  result.tangent_z = chord_length > 0.001F ? chord_z / chord_length : tangent.second;
}

void RouteFollowSystem::Steering::publish_direct_control_intent(
    const Engine::Core::Entity& entity,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementFactsComponent& facts) {
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_motion_requested) {
    return;
  }
  float const speed =
      Game::Systems::planar_length(commander->fpv_motion_vx, commander->fpv_motion_vz);
  if (speed <= 1.0e-4F) {
    return;
  }
  facts.desired.valid = true;
  facts.desired.velocity_x = commander->fpv_motion_vx;
  facts.desired.velocity_z = commander->fpv_motion_vz;
  facts.desired.tangent_x = commander->fpv_motion_vx / speed;
  facts.desired.tangent_z = commander->fpv_motion_vz / speed;
  facts.desired.heading_x = facts.desired.tangent_x;
  facts.desired.heading_z = facts.desired.tangent_z;
  facts.desired.lookahead_x = transform.position.x + commander->fpv_motion_vx;
  facts.desired.lookahead_z = transform.position.z + commander->fpv_motion_vz;
  facts.desired.speed_limit = speed;
  facts.desired.source = Engine::Core::DesiredMotionSource::DirectControl;
}

void RouteFollowSystem::Steering::publish_route_facts(FollowFrame& frame) {
  auto* facts = &frame.facts;
  auto* movement = &frame.movement;
  auto& world = frame.world;
  auto const id = frame.entity.get_id();
  facts->route.has_goal = movement->get_has_target();
  facts->route.command_sequence = movement->get_order_sequence();
  facts->route.route_id = movement->get_route_id();
  facts->route.route_revision = movement->get_route_revision();
  facts->route.topology_revision = movement->get_topology_revision();
  facts->route.lane_offset = movement->get_route_lane_offset();
  facts->route.lane_scale = movement->get_route_lane_scale();
  facts->route.cohesion_pace = 0.0F;
  const auto* membership =
      world.try_get<Engine::Core::ArmyFormationMembershipComponent>(id);
  if (membership != nullptr && membership->is_valid()) {
    const auto* formation =
        Game::Formation::ArmyFormationRegistry::for_world(world).find(
            membership->group_id);
    if (formation != nullptr) {
      facts->route.cohesion_pace = formation->cohesion_pace;
    }
  }
  if (facts->route.cohesion_pace <= 0.0F) {
    facts->route.cohesion_pace = movement->get_declared_group_pace();
  }
  facts->route.requested_goal_x = movement->get_requested_goal_x();
  facts->route.requested_goal_z = movement->get_requested_goal_z();
  facts->route.resolved_goal_x = movement->get_goal_x();
  facts->route.resolved_goal_z = movement->get_goal_y();
}

auto RouteFollowSystem::Steering::arrive_radius_for(const FollowFrame& frame,
                                                    float max_speed) -> float {
  float const waypoint_arrive_radius =
      std::clamp(max_speed * frame.delta_time * 2.0F, 0.05F, 0.25F);
  return frame.movement.get_precise_arrival()
             ? waypoint_arrive_radius
             : std::max(waypoint_arrive_radius,
                        std::clamp(CommandService::get_unit_radius(
                                       frame.world, frame.entity.get_id()) *
                                       1.1F,
                                   0.25F,
                                   0.9F));
}

auto RouteFollowSystem::Steering::aim_along_route(FollowFrame& frame,
                                                  MovementRoute& route,
                                                  float max_speed) -> RouteAim {
  RouteAim result;
  result.aim_x = frame.movement.get_target_x();
  result.aim_z = frame.movement.get_target_y();
  result.endpoint_x = frame.movement.get_target_x();
  result.endpoint_z = frame.movement.get_target_y();

  if (route.valid()) {
    aim_on_route(frame, route, max_speed, result);
  } else {
    result.remaining =
        Game::Systems::planar_length(result.endpoint_x - frame.transform.position.x,
                                     result.endpoint_z - frame.transform.position.z);
    frame.facts.progress.lateral_route_error = 0.0F;
  }
  return result;
}

void RouteFollowSystem::Steering::publish_desired_motion(FollowFrame& frame,
                                                         RouteAim aim,
                                                         float arrive_radius,
                                                         float max_speed) {
  auto* transform = &frame.transform;
  auto* facts = &frame.facts;
  auto& world = frame.world;
  auto const id = frame.entity.get_id();
  float const aim_x = aim.aim_x;
  float const aim_z = aim.aim_z;
  float const remaining = aim.remaining;
  float tangent_x = aim.tangent_x;
  float tangent_z = aim.tangent_z;

  float const dx = aim_x - transform->position.x;
  float const dz = aim_z - transform->position.z;
  float const distance = Game::Systems::planar_length(dx, dz);
  float const nx = dx / std::max(0.0001F, distance);
  float const nz = dz / std::max(0.0001F, distance);
  if (tangent_x == 0.0F && tangent_z == 0.0F) {
    tangent_x = nx;
    tangent_z = nz;
  }

  float desired_speed = max_speed;
  auto const* move_attack = world.try_get<Engine::Core::AttackComponent>(id);
  bool const ranged_mode =
      (move_attack != nullptr) && move_attack->can_ranged &&
      move_attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged;
  float const slow_radius = ranged_mode ? arrive_radius : arrive_radius * 1.5F;
  if (remaining < slow_radius) {
    desired_speed = max_speed * (remaining / slow_radius);
  }

  facts->desired.valid = true;
  facts->desired.source = Engine::Core::DesiredMotionSource::Route;
  facts->desired.velocity_x = nx * desired_speed;
  facts->desired.velocity_z = nz * desired_speed;
  facts->desired.tangent_x = tangent_x;
  facts->desired.tangent_z = tangent_z;
  facts->desired.heading_x = tangent_x;
  facts->desired.heading_z = tangent_z;
  facts->desired.lookahead_x = aim_x;
  facts->desired.lookahead_z = aim_z;
  facts->desired.speed_limit = max_speed;
}

} // namespace Game::Systems
