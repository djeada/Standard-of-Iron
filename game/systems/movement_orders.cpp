#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
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

auto find_recovery_cell(const Pathfinding& pathfinder,
                        const Point& origin,
                        Pathfinding::Passability passability,
                        Point& recovery_cell) -> bool {
  for (int radius = 1; radius <= k_recovery_search_radius; ++radius) {
    bool found_candidate = false;
    float best_distance_sq = std::numeric_limits<float>::infinity();
    Point best_candidate{};

    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dy) != radius) {
          continue;
        }

        int const check_x = origin.x + dx;
        int const check_y = origin.y + dy;
        if (!pathfinder.is_world_position_walkable(
                pathfinder.grid_to_world({check_x, check_y}), passability, 0.0F)) {
          continue;
        }

        auto const distance_sq = static_cast<float>(dx * dx + dy * dy);
        if (distance_sq < best_distance_sq) {
          best_distance_sq = distance_sq;
          best_candidate = {check_x, check_y};
          found_candidate = true;
        }
      }
    }

    if (found_candidate) {
      recovery_cell = best_candidate;
      return true;
    }
  }

  return false;
}

auto resolve_walkable_direct_target(const QVector3D& target,
                                    Pathfinding::Passability passability) -> QVector3D {
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return target;
  }
  pathfinder->update_navigation_grid();
  if (pathfinder->is_world_position_walkable(target, passability, 0.0F)) {
    return target;
  }

  Point const target_grid = NavGrid::world_to_grid(target.x(), target.z());
  for (int radius = 1; radius <= 64; ++radius) {
    for (int dz = -radius; dz <= radius; ++dz) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dz) != radius) {
          continue;
        }
        Point const candidate{target_grid.x + dx, target_grid.y + dz};
        QVector3D const world = pathfinder->grid_to_world(candidate);
        if (pathfinder->is_world_position_walkable(world, passability, 0.0F)) {
          return world;
        }
      }
    }
  }
  return target;
}

auto should_include_resolved_start_waypoint(const Point& start) -> bool {
  return !NavGrid::is_grid_walkable(start);
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
                           bool final_waypoint) -> QVector3D {
  if (final_waypoint) {
    return waypoint;
  }
  auto& terrain = Game::Map::TerrainService::instance();
  if (auto const aligned =
          terrain.get_bridge_traversal_position(waypoint.x(), waypoint.z())) {
    return {aligned->x(), waypoint.y(), aligned->z()};
  }
  if (auto const aligned =
          terrain.get_hill_entrance_traversal_position(waypoint.x(), waypoint.z())) {
    return {aligned->x(), waypoint.y(), aligned->z()};
  }
  return waypoint;
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
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    movement = entity->add_component<Engine::Core::MovementComponent>();
  }
  if (movement == nullptr) {
    return {};
  }
  movement->set_navigation_clearance(
      FormationCombat::formation_navigation_clearance(*entity));

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

void MovementSystem::assign_direct_target(Engine::Core::MovementComponent& movement,
                                          const QVector3D& target) {
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
  constexpr float skip_threshold_sq = CommandService::WAYPOINT_SKIP_THRESHOLD_SQ;
  if (path_points.size() <= 1) {
    return false;
  }

  movement.clear_path();
  std::size_t const first_waypoint_index = include_first_waypoint ? 0U : 1U;
  movement.path.reserve(path_points.size() - first_waypoint_index);
  for (std::size_t idx = first_waypoint_index; idx < path_points.size(); ++idx) {
    QVector3D const raw_waypoint =
        pathfinder.path_waypoint_world_position(path_points[idx]);
    QVector3D const waypoint =
        align_portal_waypoint(raw_waypoint, idx + 1U == path_points.size());
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

  pull_path_taut(pathfinder,
                 transform,
                 passability_for(movement),
                 movement.get_navigation_clearance(),
                 movement.path);

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
  QVector3D const resolved_goal =
      pathfinder.path_waypoint_world_position(path_points.back());
  movement.target_x = wp.first;
  movement.target_y = wp.second;
  movement.goal_x = resolved_goal.x();
  movement.goal_y = resolved_goal.z();
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
    if (moved_x * moved_x + moved_z * moved_z <=
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

  auto const path = pathfinder->find_path(
      start, end, passability_for(movement), movement.get_navigation_clearance());
  bool const include_first_waypoint = should_include_resolved_start_waypoint(start) ||
                                      (!path.empty() && path.front() != start);
  if (!assign_path_to_movement(
          *pathfinder, path, transform, movement, include_first_waypoint)) {
    QVector3D const fallback =
        path.empty() ? resolve_walkable_direct_target(requested_target,
                                                      passability_for(movement))
                     : pathfinder->path_waypoint_world_position(path.back());
    assign_direct_target(movement, fallback);
  }
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

  std::size_t const leader_index =
      static_cast<std::size_t>(std::distance(prepared.begin(), leader_it));
  QVector3D const leader_start(
      leader_it->transform->position.x, 0.0F, leader_it->transform->position.z);
  QVector3D const leader_target = targets[leader_index];
  Point const leader_start_cell =
      NavGrid::world_to_grid(leader_start.x(), leader_start.z());
  Point const leader_target_cell =
      NavGrid::world_to_grid(leader_target.x(), leader_target.z());

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

  bool const leader_direct =
      is_direct_path_walkable(
          leader_start, leader_target, group_passability, group_clearance) &&
      (leader_start_cell == leader_target_cell ||
       !segment_traverses_navigation_portal(leader_start, leader_target));
  std::vector<Point> corridor;
  if (!leader_direct) {
    corridor = pathfinder->find_path(
        leader_start_cell, leader_target_cell, group_passability, group_clearance);
  }

  constexpr int k_shared_corridor_region_radius = 16;
  std::size_t synchronous_fallbacks = 0;
  for (std::size_t i = 0; i < prepared.size(); ++i) {
    auto& move = prepared[i];
    if (move.transform == nullptr || move.movement == nullptr) {
      continue;
    }

    bool assigned = false;
    if (corridor.size() > 1U) {
      Point const start = NavGrid::world_to_grid(move.transform->position.x,
                                                 move.transform->position.z);
      Point const target = NavGrid::world_to_grid(targets[i].x(), targets[i].z());
      bool const same_regions =
          std::abs(start.x - leader_start_cell.x) <= k_shared_corridor_region_radius &&
          std::abs(start.y - leader_start_cell.y) <= k_shared_corridor_region_radius &&
          std::abs(target.x - leader_target_cell.x) <=
              k_shared_corridor_region_radius &&
          std::abs(target.y - leader_target_cell.y) <= k_shared_corridor_region_radius;
      QVector3D const corridor_exit =
          pathfinder->path_waypoint_world_position(corridor.back());
      std::size_t const entry_index =
          should_include_resolved_start_waypoint(start) ? 0U : 1U;
      QVector3D const corridor_entry =
          pathfinder->path_waypoint_world_position(corridor[entry_index]);
      QVector3D const current(
          move.transform->position.x, 0.0F, move.transform->position.z);
      if (same_regions &&
          pathfinder->is_world_segment_walkable(
              current,
              corridor_entry,
              passability_for(*move.movement),
              move.movement->get_navigation_clearance()) &&
          pathfinder->is_world_segment_walkable(
              corridor_exit,
              targets[i],
              passability_for(*move.movement),
              move.movement->get_navigation_clearance())) {
        assigned =
            assign_path_to_movement(*pathfinder,
                                    corridor,
                                    *move.transform,
                                    *move.movement,
                                    should_include_resolved_start_waypoint(start));
        if (assigned) {
          QVector3D const resolved_target = resolve_walkable_direct_target(
              targets[i], passability_for(*move.movement));
          if (std::hypot(resolved_target.x() - corridor_exit.x(),
                         resolved_target.z() - corridor_exit.z()) > 0.01F) {
            move.movement->path.emplace_back(resolved_target.x(), resolved_target.z());
          }
          move.movement->goal_x = resolved_target.x();
          move.movement->goal_y = resolved_target.z();
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
                     pathfinder->navigation_revision())) {
        move.movement->stop();
        move.movement->set_rest_position(targets[i].x(), targets[i].z());
      } else if (corridor.size() > 1U) {
        assigned = assign_path_to_movement(
            *pathfinder, corridor, *move.transform, *move.movement, false);
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

  for (const auto& intent : intents) {
    issue_move(world, intent.unit_id, intent.target, options);
  }
}

} // namespace Game::Systems
