#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "combat_rules.h"
#include "command_service.h"
#include "formation_combat_geometry.h"
#include "movement_system.h"
#include "nav_grid.h"
#include "order_service.h"
#include "pathfinding.h"
#include "route_corridor_planner.h"
#include "walkability.h"

namespace Game::Systems {

namespace {
constexpr float same_target_threshold_sq = 0.01F;

constexpr int k_recovery_search_radius = 16;
constexpr float k_route_keep_goal_shift = 1.5F;

auto passability_for(const Engine::Core::MovementComponent& movement)
    -> Pathfinding::Passability {
  return movement.get_can_enter_forest() ? Pathfinding::Passability::Light
                                         : Pathfinding::Passability::Heavy;
}

auto is_direct_path_walkable(const QVector3D& from,
                             const QVector3D& to,
                             Pathfinding::Passability passability,
                             float clearance_radius) -> bool {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->update_navigation_grid();
    return pathfinder->is_world_segment_walkable(
        from, to, passability, clearance_radius);
  }

  return NavGrid::is_world_position_walkable(to);
}

auto nearest_standable_world(const QVector3D& position,
                             Pathfinding::Passability passability,
                             float search_radius) -> std::optional<QVector3D> {
  BodyProfile profile;
  profile.radius = 0.0F;
  profile.passability = passability;
  return Walkability::nearest_standable(position, profile, search_radius);
}

auto find_recovery_cell(const Pathfinding& pathfinder,
                        const Point& origin,
                        Pathfinding::Passability passability,
                        Point& recovery_cell) -> bool {
  auto const spot =
      nearest_standable_world(pathfinder.grid_to_world(origin),
                              passability,
                              static_cast<float>(k_recovery_search_radius));
  if (!spot.has_value()) {
    return false;
  }
  recovery_cell = NavGrid::world_to_grid(spot->x(), spot->z());
  return true;
}

auto resolve_walkable_direct_target(const QVector3D& target,
                                    Pathfinding::Passability passability) -> QVector3D {
  constexpr float k_target_search_radius = 64.0F;
  return nearest_standable_world(target, passability, k_target_search_radius)
      .value_or(target);
}

auto segment_traverses_navigation_portal(const QVector3D& from,
                                         const QVector3D& to) -> bool {
  auto& terrain = Game::Map::TerrainService::instance();
  auto const* height_map = terrain.get_height_map();
  if (height_map == nullptr) {
    return false;
  }

  QVector3D const delta = to - from;
  float const length = std::hypot(delta.x(), delta.z());
  float const sample_step = std::max(height_map->get_tile_size() * 0.5F, 0.25F);
  int const sample_count =
      std::max(1, static_cast<int>(std::ceil(length / sample_step)));
  for (int sample = 0; sample <= sample_count; ++sample) {
    float const t = static_cast<float>(sample) / static_cast<float>(sample_count);
    QVector3D const point = from + delta * t;
    Point const cell = NavGrid::world_to_grid(point.x(), point.z());
    if (terrain.is_on_bridge(point.x(), point.z()) ||
        terrain.is_hill_entrance(cell.x, cell.y)) {
      return true;
    }
  }
  return false;
}

auto align_portal_waypoint(const QVector3D& waypoint,
                           bool final_waypoint,
                           const std::optional<QVector3D>& previous) -> QVector3D {
  if (final_waypoint) {
    return waypoint;
  }

  const auto* pathfinder = NavGrid::get_pathfinder();
  auto usable = [pathfinder, &previous](const QVector3D& candidate) {
    if (pathfinder == nullptr) {
      return true;
    }
    Point const cell = NavGrid::world_to_grid(candidate.x(), candidate.z());
    if (!pathfinder->is_walkable(cell.x, cell.y)) {
      return false;
    }

    return !previous.has_value() ||
           pathfinder->is_world_segment_walkable(
               *previous, candidate, Pathfinding::Passability::Light, 0.0F);
  };

  auto& terrain = Game::Map::TerrainService::instance();
  if (auto const aligned =
          terrain.get_bridge_traversal_position(waypoint.x(), waypoint.z())) {
    QVector3D const candidate(aligned->x(), waypoint.y(), aligned->z());
    if (usable(candidate)) {
      return candidate;
    }
  }
  if (auto const aligned =
          terrain.get_hill_entrance_traversal_position(waypoint.x(), waypoint.z())) {
    QVector3D const candidate(aligned->x(), waypoint.y(), aligned->z());
    if (usable(candidate)) {
      return candidate;
    }
  }
  return waypoint;
}

[[nodiscard]] auto
path_legs_are_walkable(Pathfinding& pathfinder,
                       const Engine::Core::TransformComponent& transform,
                       Pathfinding::Passability passability,
                       const std::vector<std::pair<float, float>>& path) -> bool {
  QVector3D previous(transform.position.x, 0.0F, transform.position.z);
  for (auto const& waypoint : path) {
    QVector3D const point(waypoint.first, 0.0F, waypoint.second);
    if (!pathfinder.is_world_segment_walkable(previous, point, passability, 0.0F)) {
      return false;
    }
    previous = point;
  }
  return true;
}

void pull_path_taut(Pathfinding& pathfinder,
                    const Engine::Core::TransformComponent& transform,
                    Pathfinding::Passability passability,
                    float clearance_radius,
                    std::vector<std::pair<float, float>>& path) {
  if (path.size() < 3U) {
    return;
  }
  auto shortcut_allowed = [&](const QVector3D& from, const QVector3D& to) -> bool {
    return pathfinder.is_world_segment_walkable(
               from, to, passability, clearance_radius) &&
           !segment_traverses_navigation_portal(from, to);
  };

  std::vector<std::pair<float, float>> taut;
  taut.reserve(path.size());
  QVector3D anchor(transform.position.x, 0.0F, transform.position.z);
  std::size_t index = 0;
  while (index < path.size()) {
    std::size_t reach = index;
    while (reach + 1U < path.size()) {
      QVector3D const candidate(path[reach + 1U].first, 0.0F, path[reach + 1U].second);
      if (!shortcut_allowed(anchor, candidate)) {
        break;
      }
      ++reach;
    }
    taut.push_back(path[reach]);
    anchor = QVector3D(path[reach].first, 0.0F, path[reach].second);
    index = reach + 1U;
  }
  path = std::move(taut);
}

struct PreparedMove {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  float previous_vx{0.0F};
  float previous_vz{0.0F};
  bool preserve_velocity{false};
};

auto prepare_move(Engine::Core::World& world,
                  Engine::Core::EntityID unit_id,
                  const CommandService::MoveOptions& options) -> PreparedMove {
  auto* entity = world.get_entity(unit_id);
  if (entity == nullptr) {
    return {};
  }

  auto* attack = entity->get_component<Engine::Core::AttackComponent>();
  if (attack != nullptr && attack->in_melee_lock &&
      CombatRules::participates_in_rts_melee_lock(entity)) {
    auto* locked_target = world.get_entity(attack->melee_lock_target_id);
    auto const* locked_unit =
        locked_target != nullptr
            ? locked_target->get_component<Engine::Core::UnitComponent>()
            : nullptr;

    bool const locked_to_structure =
        locked_target != nullptr &&
        locked_target->has_component<Engine::Core::BuildingComponent>();
    bool const opponent_alive =
        locked_unit != nullptr && locked_unit->health > 0 &&
        !locked_target->has_component<Engine::Core::PendingRemovalComponent>();
    if (opponent_alive && !locked_to_structure) {
      return {};
    }
    CombatRules::clear_rts_melee_lock(entity);
  }

  OrderService::prepare_for_move(entity, options.kind, options.preserve_formation_mode);
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return {};
  }
  auto* movement =
      Engine::Core::get_or_add_component<Engine::Core::MovementComponent>(entity);
  if (movement == nullptr) {
    return {};
  }
  movement->set_navigation_clearance(
      FormationCombat::formation_navigation_clearance(*entity));

  movement->begin_order();

  PreparedMove result;
  result.entity = entity;
  result.transform = transform;
  result.movement = movement;
  result.previous_vx = movement->get_vx();
  result.previous_vz = movement->get_vz();
  result.preserve_velocity =
      options.kind == MoveOrderKind::AttackChase && movement->get_has_target();
  return result;
}

} // namespace

namespace {

void stamp_route_revision(Engine::Core::MovementComponent& movement) {
  auto const* pathfinder = NavGrid::get_pathfinder();
  movement.begin_route(pathfinder != nullptr ? pathfinder->navigation_revision() : 0U);
}

} // namespace

void MovementSystem::assign_direct_target(Engine::Core::MovementComponent& movement,
                                          const QVector3D& target) {
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
  movement.vx = 0.0F;
  movement.vz = 0.0F;
}

auto MovementSystem::assign_path_to_movement(
    Pathfinding& pathfinder,
    const std::vector<Point>& path_points,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement,
    bool include_first_waypoint) -> bool {
  if (path_points.size() <= 1) {
    return false;
  }

  std::size_t const first_waypoint_index = include_first_waypoint ? 0U : 1U;
  std::vector<QVector3D> waypoints;
  waypoints.reserve(path_points.size() - first_waypoint_index);
  for (std::size_t idx = first_waypoint_index; idx < path_points.size(); ++idx) {
    QVector3D const raw_waypoint =
        pathfinder.path_waypoint_world_position(path_points[idx]);
    QVector3D const waypoint = align_portal_waypoint(
        raw_waypoint,
        idx + 1U == path_points.size(),
        waypoints.empty() ? std::optional<QVector3D>{}
                          : std::optional<QVector3D>{waypoints.back()});
    if (!waypoints.empty()) {
      QVector3D const delta = waypoint - waypoints.back();
      float const dx = delta.x();
      float const dz = delta.z();
      if (dx * dx + dz * dz <= 1.0e-6F) {
        continue;
      }
    }
    waypoints.push_back(waypoint);
  }

  QVector3D const resolved_goal =
      pathfinder.path_waypoint_world_position(path_points.back());
  return assign_waypoints_to_movement(
      pathfinder, waypoints, resolved_goal, transform, movement);
}

auto MovementSystem::assign_waypoints_to_movement(
    Pathfinding& pathfinder,
    const std::vector<QVector3D>& waypoints,
    const QVector3D& resolved_goal,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement) -> bool {
  constexpr float skip_threshold_sq = CommandService::WAYPOINT_SKIP_THRESHOLD_SQ;
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

  if (!path_legs_are_walkable(
          pathfinder, transform, passability_for(movement), movement.path)) {
    movement.path = plain;
  }

  {

    auto const cornered = movement.path;
    pull_path_taut(pathfinder,
                   transform,
                   passability_for(movement),
                   movement.get_navigation_clearance(),
                   movement.path);
    if (!path_legs_are_walkable(
            pathfinder, transform, passability_for(movement), movement.path)) {
      movement.path = cornered;
    }
  }

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
  movement.vx = 0.0F;
  movement.vz = 0.0F;
  return true;
}

void MovementSystem::assign_navigation_target(
    Pathfinding* pathfinder,
    const Engine::Core::TransformComponent& transform,
    Engine::Core::MovementComponent& movement,
    const QVector3D& requested_target) {
  if (movement.has_requested_goal && movement.has_target && movement.has_waypoints() &&
      movement.remaining_waypoints() > 1U) {
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
      return;
    }
  }
  movement.requested_goal_x = requested_target.x();
  movement.requested_goal_z = requested_target.z();
  movement.has_requested_goal = true;

  if (pathfinder == nullptr) {
    assign_direct_target(movement, requested_target);
    return;
  }

  Point const start =
      NavGrid::world_to_grid(transform.position.x, transform.position.z);
  Point const end = NavGrid::world_to_grid(requested_target.x(), requested_target.z());
  QVector3D const current_pos(transform.position.x, 0.0F, transform.position.z);

  bool const portal_route =
      segment_traverses_navigation_portal(current_pos, requested_target);
  bool const direct_clear =
      is_direct_path_walkable(current_pos,
                              requested_target,
                              passability_for(movement),
                              movement.get_navigation_clearance());
  if ((start == end && direct_clear) || (direct_clear && !portal_route)) {
    assign_direct_target(
        movement,
        resolve_walkable_direct_target(requested_target, passability_for(movement)));
    return;
  }

  auto const corridor = RouteCorridorPlanner::plan(*pathfinder,
                                                   current_pos,
                                                   requested_target,
                                                   passability_for(movement),
                                                   movement.get_navigation_clearance());
  if (!corridor.reachable() || !assign_waypoints_to_movement(*pathfinder,
                                                             corridor.centerline,
                                                             corridor.centerline.back(),
                                                             transform,
                                                             movement)) {
    QVector3D const fallback = corridor.centerline.empty()
                                   ? resolve_walkable_direct_target(
                                         requested_target, passability_for(movement))
                                   : corridor.centerline.back();
    assign_direct_target(movement, fallback);
  }

  movement.requested_goal_x = requested_target.x();
  movement.requested_goal_z = requested_target.z();
  movement.has_requested_goal = true;
}

auto MovementSystem::assign_local_recovery_move(
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
  return true;
}

auto MovementSystem::retarget_unit(Engine::Core::World& world,
                                   Engine::Core::EntityID entity_id,
                                   const QVector3D& goal) -> bool {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return false;
  }

  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    return false;
  }

  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return false;
  }

  assign_navigation_target(NavGrid::get_pathfinder(), *transform, *movement, goal);
  return true;
}

void MovementSystem::issue_move(Engine::Core::World& world,
                                Engine::Core::EntityID unit_id,
                                const QVector3D& target) {
  issue_move(world, unit_id, target, MoveOptions{});
}

void MovementSystem::issue_move(Engine::Core::World& world,
                                Engine::Core::EntityID unit_id,
                                const QVector3D& target,
                                const MoveOptions& options) {
  PreparedMove const prepared = prepare_move(world, unit_id, options);
  if (auto* system = world.get_system<MovementSystem>()) {
    system->cancel_pending_path_request(unit_id);
  }
  if (prepared.movement == nullptr || prepared.transform == nullptr) {
    return;
  }
  prepared.movement->precise_arrival = options.kind == MoveOrderKind::AttackChase;
  assign_navigation_target(
      NavGrid::get_pathfinder(), *prepared.transform, *prepared.movement, target);
  if (prepared.preserve_velocity && prepared.movement->get_has_target()) {
    prepared.movement->vx = prepared.previous_vx;
    prepared.movement->vz = prepared.previous_vz;
  }
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<Engine::Core::EntityID>& units,
                                      const std::vector<QVector3D>& targets) {
  issue_move_units(world, units, targets, MoveOptions{});
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<Engine::Core::EntityID>& units,
                                      const std::vector<QVector3D>& targets,
                                      const MoveOptions& options) {
  if (units.size() != targets.size()) {
    return;
  }
  if (units.empty()) {
    return;
  }

  std::vector<PreparedMove> prepared;
  prepared.reserve(units.size());
  auto* movement_system = world.get_system<MovementSystem>();
  for (Engine::Core::EntityID const unit_id : units) {
    if (movement_system != nullptr) {
      movement_system->cancel_pending_path_request(unit_id);
    }
    prepared.push_back(prepare_move(world, unit_id, options));
    if (prepared.back().movement != nullptr) {
      prepared.back().movement->precise_arrival =
          options.kind == MoveOrderKind::AttackChase;
    }
  }

  float declared_group_pace = std::numeric_limits<float>::max();
  for (auto const& move : prepared) {
    const auto* unit = move.entity != nullptr
                           ? move.entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (move.movement != nullptr && unit != nullptr && unit->speed > 0.0F) {
      declared_group_pace = std::min(declared_group_pace, unit->speed);
    }
  }
  if (std::isfinite(declared_group_pace)) {
    for (auto& move : prepared) {
      if (move.movement != nullptr) {
        move.movement->declared_group_pace = declared_group_pace;
      }
    }
  }

  auto* pathfinder = NavGrid::get_pathfinder();
  auto const leader_it =
      std::find_if(prepared.begin(), prepared.end(), [](PreparedMove const& move) {
        return move.transform != nullptr && move.movement != nullptr;
      });
  if (pathfinder == nullptr || leader_it == prepared.end() || units.size() < 2U) {
    for (std::size_t i = 0; i < prepared.size(); ++i) {
      if (prepared[i].transform != nullptr && prepared[i].movement != nullptr) {
        assign_navigation_target(
            pathfinder, *prepared[i].transform, *prepared[i].movement, targets[i]);
      }
    }
    return;
  }

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
  QVector3D const slot_center = group_target;
  group_start = NavGrid::snap_to_walkable_ground(group_start, 16);
  group_target = NavGrid::snap_to_walkable_ground(group_target, 16);
  auto const corridor = RouteCorridorPlanner::plan(
      *pathfinder, group_start, group_target, group_passability, group_clearance);

  QVector3D final_tangent(0.0F, 0.0F, 1.0F);
  if (corridor.reachable()) {
    for (std::size_t index = corridor.centerline.size() - 1U; index > 0U; --index) {
      final_tangent = corridor.centerline[index] - corridor.centerline[index - 1U];
      final_tangent.setY(0.0F);
      if (final_tangent.lengthSquared() > 1.0e-6F) {
        final_tangent.normalize();
        break;
      }
    }
  }
  QVector3D const final_right(final_tangent.z(), 0.0F, -final_tangent.x());

  std::size_t synchronous_fallbacks = 0;
  for (std::size_t i = 0; i < prepared.size(); ++i) {
    auto& move = prepared[i];
    if (move.transform == nullptr || move.movement == nullptr) {
      continue;
    }

    bool assigned = false;
    if (corridor.reachable()) {
      QVector3D const current(
          move.transform->position.x, 0.0F, move.transform->position.z);
      QVector3D const target_offset = targets[i] - slot_center;
      float const lateral_offset = QVector3D::dotProduct(target_offset, final_right);
      auto const lane =
          RouteCorridorPlanner::fit_lane(*pathfinder,
                                         corridor,
                                         current,
                                         targets[i],
                                         lateral_offset,
                                         passability_for(*move.movement),
                                         move.movement->get_navigation_clearance());
      if (lane.valid()) {
        assigned = assign_waypoints_to_movement(
            *pathfinder, lane.waypoints, targets[i], *move.transform, *move.movement);
        if (assigned) {
          move.movement->route_id = corridor.id;
          move.movement->route_lane_offset = lateral_offset;
          move.movement->route_lane_min_scale = lane.minimum_lateral_scale;
          auto nearest_waypoint = [&](const QVector3D& point) {
            std::size_t nearest = 0U;
            float nearest_distance_sq = std::numeric_limits<float>::max();
            for (std::size_t waypoint = 0; waypoint < move.movement->path.size();
                 ++waypoint) {
              float const dx = move.movement->path[waypoint].first - point.x();
              float const dz = move.movement->path[waypoint].second - point.z();
              float const distance_sq = dx * dx + dz * dz;
              if (distance_sq < nearest_distance_sq) {
                nearest = waypoint;
                nearest_distance_sq = distance_sq;
              }
            }
            return nearest;
          };
          if (lane.opening_point.has_value() && lane.reform_point.has_value()) {
            move.movement->route_opening_waypoint_index =
                nearest_waypoint(*lane.opening_point);
            move.movement->route_reform_waypoint_index =
                std::max(move.movement->route_opening_waypoint_index + 1U,
                         nearest_waypoint(*lane.reform_point));
          }
        }
      }
    }
    if (!assigned) {
      QVector3D const current(
          move.transform->position.x, 0.0F, move.transform->position.z);
      Point const start = NavGrid::world_to_grid(current.x(), current.z());
      Point const target = NavGrid::world_to_grid(targets[i].x(), targets[i].z());
      bool const direct_clear =
          is_direct_path_walkable(current,
                                  targets[i],
                                  passability_for(*move.movement),
                                  move.movement->get_navigation_clearance());
      bool const direct =
          direct_clear && (start == target ||
                           !segment_traverses_navigation_portal(current, targets[i]));
      if (direct || movement_system == nullptr ||
          synchronous_fallbacks < k_path_requests_per_tick) {
        assign_navigation_target(
            pathfinder, *move.transform, *move.movement, targets[i]);
        synchronous_fallbacks += direct ? 0U : 1U;
      } else if (movement_system != nullptr &&
                 movement_system->enqueue_pending_path_request(
                     move.entity->get_id(),
                     targets[i],
                     options.kind == MoveOrderKind::AttackChase,
                     pathfinder->navigation_revision(),
                     move.movement->get_order_sequence())) {
        move.movement->stop();
        move.movement->set_rest_position(targets[i].x(), targets[i].z());
      }
    }
    if (move.preserve_velocity && move.movement->get_has_target()) {
      move.movement->vx = move.previous_vx;
      move.movement->vz = move.previous_vz;
    }
  }
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<MoveIntent>& intents) {
  issue_move_units(world, intents, MoveOptions{});
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<MoveIntent>& intents,
                                      const MoveOptions& options) {
  std::vector<Engine::Core::EntityID> units;
  std::vector<QVector3D> targets;
  units.reserve(intents.size());
  targets.reserve(intents.size());
  for (const auto& intent : intents) {
    units.push_back(intent.unit_id);
    targets.push_back(intent.target);
    if (!intent.facing_angle.has_value()) {
      continue;
    }
    auto* entity = world.get_entity(intent.unit_id);
    auto const* formation_mode =
        entity != nullptr
            ? entity->get_component<Engine::Core::FormationModeComponent>()
            : nullptr;
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (formation_mode != nullptr && formation_mode->active && transform != nullptr) {
      transform->desired_yaw = *intent.facing_angle;
      transform->has_desired_yaw = true;
    }
  }
  if (options.kind == MoveOrderKind::AttackChase) {
    for (const auto& intent : intents) {
      issue_move(world, intent.unit_id, intent.target, options);
    }
    return;
  }
  issue_move_units(world, units, targets, options);
}

} // namespace Game::Systems
