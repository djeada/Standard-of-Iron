#include "movement_orders_group.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "movement_orders_assignment.h"
#include "movement_orders_prepared.h"
#include "movement_orders_targets.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "systems/navigation/route_corridor_planner.h"

namespace Game::Systems {

using namespace MovementTargeting;

namespace {

constexpr float k_own_route_detour = 1.35F;
constexpr float k_own_route_slack_metres = 2.0F;

} // namespace

struct MovementSystem::GroupIssue::GroupRoute {
  RouteCorridorPlan corridor;
  QVector3D slot_center;
  QVector3D final_right;
};

auto MovementSystem::GroupIssue::prepare_all(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& units,
    const CommandService::MoveOptions& options,
    PathRequestQueue* path_requests) -> std::vector<PreparedMove> {
  std::vector<PreparedMove> prepared;
  prepared.reserve(units.size());
  for (Engine::Core::EntityID const unit_id : units) {
    if (path_requests != nullptr) {
      path_requests->cancel(unit_id);
    }
    prepared.push_back(prepare_move(world, unit_id, options));
    if (prepared.back().movement != nullptr) {
      prepared.back().movement->precise_arrival =
          options.kind == MoveOrderKind::AttackChase ||
          options.kind == MoveOrderKind::FormationMove;
      prepared.back().movement->issuer_retargets = issuer_retargets(options.kind);
    }
  }
  return prepared;
}

void MovementSystem::GroupIssue::declare_group_pace(
    Engine::Core::World& world, std::vector<PreparedMove>& prepared) {
  float declared_group_pace = std::numeric_limits<float>::max();
  for (auto const& move : prepared) {
    const auto* unit =
        move.entity != nullptr
            ? world.try_get<Engine::Core::UnitComponent>(move.entity->get_id())
            : nullptr;
    if (move.movement != nullptr && unit != nullptr && unit->speed > 0.0F) {
      declared_group_pace = std::min(declared_group_pace, unit->speed);
    }
  }
  if (!std::isfinite(declared_group_pace)) {
    return;
  }
  for (auto& move : prepared) {
    if (move.movement != nullptr) {
      move.movement->declared_group_pace = declared_group_pace;
    }
  }
}

void MovementSystem::GroupIssue::route_individually(
    Pathfinding* pathfinder,
    std::vector<PreparedMove>& prepared,
    const std::vector<QVector3D>& targets) {
  for (std::size_t i = 0; i < prepared.size(); ++i) {
    if (prepared[i].transform != nullptr && prepared[i].movement != nullptr) {
      MovementSystem::Assignment::assign_navigation_target(
          pathfinder, *prepared[i].transform, *prepared[i].movement, targets[i]);
    }
  }
}

auto MovementSystem::GroupIssue::plan_group_route(
    Pathfinding& pathfinder,
    const std::vector<PreparedMove>& prepared,
    const std::vector<QVector3D>& targets) -> GroupRoute {
  auto group_passability = Pathfinding::Passability::Light;
  float group_clearance = 0.0F;
  for (auto const& move : prepared) {
    if (move.movement != nullptr && !move.movement->get_can_enter_forest()) {
      group_passability = Pathfinding::Passability::Heavy;
    }
    if (move.movement != nullptr) {
      group_clearance =
          std::max(group_clearance, move.movement->get_navigation_clearance());
    }
  }

  std::vector<std::size_t> canonical_members;
  canonical_members.reserve(prepared.size());
  for (std::size_t index = 0; index < prepared.size(); ++index) {
    if (prepared[index].entity != nullptr && prepared[index].transform != nullptr &&
        prepared[index].movement != nullptr) {
      canonical_members.push_back(index);
    }
  }
  std::sort(canonical_members.begin(),
            canonical_members.end(),
            [&](std::size_t lhs, std::size_t rhs) {
              return prepared[lhs].entity->get_id() < prepared[rhs].entity->get_id();
            });

  QVector3D group_start;
  QVector3D group_target;
  for (std::size_t const index : canonical_members) {
    group_start += QVector3D(prepared[index].transform->position.x,
                             0.0F,
                             prepared[index].transform->position.z);
    group_target += targets[index];
  }
  float const member_count = static_cast<float>(canonical_members.size());
  group_start /= member_count;
  group_target /= member_count;
  GroupRoute route;
  route.slot_center = group_target;
  group_start = NavGrid::snap_to_walkable_ground(group_start, 16);
  group_target = NavGrid::snap_to_walkable_ground(group_target, 16);
  route.corridor = RouteCorridorPlanner::plan(
      pathfinder, group_start, group_target, group_passability, group_clearance);

  QVector3D final_tangent(0.0F, 0.0F, 1.0F);
  if (route.corridor.reachable()) {
    for (std::size_t index = route.corridor.centerline.size() - 1U; index > 0U;
         --index) {
      final_tangent =
          route.corridor.centerline[index] - route.corridor.centerline[index - 1U];
      final_tangent.setY(0.0F);
      if (final_tangent.lengthSquared() > 1.0e-6F) {
        final_tangent.normalize();
        break;
      }
    }
  }
  route.final_right = QVector3D(final_tangent.z(), 0.0F, -final_tangent.x());
  return route;
}

class MovementSystem::GroupIssue::MemberRouter {
public:
  MemberRouter(Pathfinding& pathfinder,
               const GroupRoute& route,
               const CommandService::MoveOptions& options,
               PathRequestQueue* path_requests)
      : m_pathfinder(pathfinder)
      , m_route(route)
      , m_options(options)
      , m_path_requests(path_requests) {}

  void route(PreparedMove& move, const QVector3D& target) {
    QVector3D const current(
        move.transform->position.x, 0.0F, move.transform->position.z);
    QVector3D const member_target = resolve_walkable_target_toward(
        target, current, passability_for(*move.movement));

    bool assigned = try_escape(move, target, member_target);
    assigned = assigned || try_direct_formation_move(move, current, member_target);
    assigned = assigned || try_own_route(move, current, member_target);
    assigned = assigned || try_group_lane(move, current, member_target);
    if (!assigned) {
      search_for_route(move, current, target);
    }
    if (move.preserve_velocity && move.movement->get_has_target()) {
      move.movement->vx = move.previous_vx;
      move.movement->vz = move.previous_vz;
    }
  }

private:
  auto try_escape(PreparedMove& move,
                  const QVector3D& target,
                  const QVector3D& member_target) -> bool {
    if (!MovementSystem::Assignment::assign_escape_if_sealed(
            m_pathfinder, *move.transform, *move.movement, member_target)) {
      return false;
    }
    move.movement->requested_goal_x = target.x();
    move.movement->requested_goal_z = target.z();
    move.movement->has_requested_goal = true;
    return true;
  }

  auto try_direct_formation_move(PreparedMove& move,
                                 const QVector3D& current,
                                 const QVector3D& member_target) -> bool {
    if (m_options.kind != MoveOrderKind::FormationMove ||
        !is_direct_path_walkable(current,
                                 member_target,
                                 passability_for(*move.movement),
                                 move.movement->get_navigation_clearance())) {
      return false;
    }
    MovementSystem::Assignment::assign_direct_target(*move.movement, member_target);
    return true;
  }

  auto try_own_route(PreparedMove& move,
                     const QVector3D& current,
                     const QVector3D& member_target) -> bool {
    if (!m_options.prefer_own_routes) {
      return false;
    }
    auto const own =
        RouteCorridorPlanner::plan(m_pathfinder,
                                   current,
                                   member_target,
                                   passability_for(*move.movement),
                                   move.movement->get_navigation_clearance());
    if (!own.reachable()) {
      return false;
    }
    float length = 0.0F;
    QVector3D previous = current;
    for (const auto& point : own.centerline) {
      length +=
          QVector3D(point.x() - previous.x(), 0.0F, point.z() - previous.z()).length();
      previous = point;
    }
    float const straight = QVector3D(member_target.x() - current.x(),
                                     0.0F,
                                     member_target.z() - current.z())
                               .length();
    if (length > straight * k_own_route_detour + k_own_route_slack_metres) {
      return false;
    }
    return MovementSystem::Assignment::assign_waypoints_to_movement(
        m_pathfinder,
        own.centerline,
        own.centerline.back(),
        *move.transform,
        *move.movement);
  }

  static auto nearest_waypoint(const Engine::Core::MovementComponent& movement,
                               const QVector3D& point) -> std::size_t {
    std::size_t nearest = 0U;
    float nearest_distance_sq = std::numeric_limits<float>::max();
    for (std::size_t waypoint = 0; waypoint < movement.path.size(); ++waypoint) {
      float const dx = movement.path[waypoint].first - point.x();
      float const dz = movement.path[waypoint].second - point.z();
      float const distance_sq = dx * dx + dz * dz;
      if (distance_sq < nearest_distance_sq) {
        nearest = waypoint;
        nearest_distance_sq = distance_sq;
      }
    }
    return nearest;
  }

  auto try_group_lane(PreparedMove& move,
                      const QVector3D& current,
                      const QVector3D& member_target) -> bool {
    if (!m_route.corridor.reachable()) {
      return false;
    }
    QVector3D const target_offset = member_target - m_route.slot_center;
    float const lateral_offset =
        QVector3D::dotProduct(target_offset, m_route.final_right);
    auto const lane =
        RouteCorridorPlanner::fit_lane(m_pathfinder,
                                       m_route.corridor,
                                       current,
                                       member_target,
                                       lateral_offset,
                                       passability_for(*move.movement),
                                       move.movement->get_navigation_clearance());
    if (!lane.valid() ||
        !MovementSystem::Assignment::assign_waypoints_to_movement(m_pathfinder,
                                                                  lane.waypoints,
                                                                  member_target,
                                                                  *move.transform,
                                                                  *move.movement)) {
      return false;
    }
    auto& movement = *move.movement;
    movement.route_id = m_route.corridor.id;
    movement.route_lane_offset = lateral_offset;
    movement.route_lane_min_scale = lane.minimum_lateral_scale;
    if (lane.opening_point.has_value() && lane.reform_point.has_value()) {
      movement.route_opening_waypoint_index =
          nearest_waypoint(movement, *lane.opening_point);
      movement.route_reform_waypoint_index =
          std::max(movement.route_opening_waypoint_index + 1U,
                   nearest_waypoint(movement, *lane.reform_point));
    }
    return true;
  }

  void search_for_route(PreparedMove& move,
                        const QVector3D& current,
                        const QVector3D& target) {
    Point const start = NavGrid::world_to_grid(current.x(), current.z());
    Point const end = NavGrid::world_to_grid(target.x(), target.z());
    bool const direct_clear =
        is_direct_path_walkable(current,
                                target,
                                passability_for(*move.movement),
                                move.movement->get_navigation_clearance());
    bool const direct =
        direct_clear &&
        (start == end || !segment_traverses_navigation_portal(current, target));
    if (direct || m_path_requests == nullptr ||
        m_synchronous_fallbacks < PathRequestQueue::k_requests_per_tick) {
      MovementSystem::Assignment::assign_navigation_target(
          &m_pathfinder, *move.transform, *move.movement, target);
      m_synchronous_fallbacks += direct ? 0U : 1U;
    } else if (m_path_requests->enqueue(move.entity->get_id(),
                                        target,
                                        m_options.kind == MoveOrderKind::AttackChase,
                                        m_pathfinder.navigation_revision(),
                                        move.movement->get_order_sequence())) {
      move.movement->stop();
      move.movement->set_rest_position(target.x(), target.z());
    }
  }

  Pathfinding& m_pathfinder;
  const GroupRoute& m_route;
  const CommandService::MoveOptions& m_options;
  PathRequestQueue* m_path_requests;
  std::size_t m_synchronous_fallbacks{0};
};

void MovementSystem::GroupIssue::issue(Engine::Core::World& world,
                                       const std::vector<Engine::Core::EntityID>& units,
                                       const std::vector<QVector3D>& targets,
                                       const CommandService::MoveOptions& options,
                                       PathRequestQueue* path_requests) {
  if (units.size() != targets.size() || units.empty()) {
    return;
  }

  std::vector<PreparedMove> prepared =
      prepare_all(world, units, options, path_requests);
  declare_group_pace(world, prepared);

  auto* pathfinder = NavGrid::get_pathfinder();
  auto const has_leader =
      std::any_of(prepared.begin(), prepared.end(), [](PreparedMove const& move) {
        return move.transform != nullptr && move.movement != nullptr;
      });
  if (pathfinder == nullptr || !has_leader || units.size() < 2U) {
    route_individually(pathfinder, prepared, targets);
    return;
  }

  GroupRoute const route = plan_group_route(*pathfinder, prepared, targets);
  MemberRouter router(*pathfinder, route, options, path_requests);
  for (std::size_t i = 0; i < prepared.size(); ++i) {
    if (prepared[i].transform != nullptr && prepared[i].movement != nullptr) {
      router.route(prepared[i], targets[i]);
    }
  }
}

} // namespace Game::Systems
