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
#include "movement_system.h"
#include "order_service.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {
constexpr float same_target_threshold_sq = 0.01F;

constexpr int k_recovery_search_radius = 16;

auto is_direct_path_walkable(const QVector3D& from, const QVector3D& to) -> bool {
  auto* pathfinder = CommandService::get_pathfinder();
  if (pathfinder != nullptr) {
    pathfinder->update_navigation_grid();
    return pathfinder->is_world_segment_walkable(from, to);
  }

  return CommandService::is_world_position_walkable(to);
}

auto is_walkable_cell(int x, int y) -> bool {
  return CommandService::is_grid_walkable({x, y});
}

auto find_recovery_cell(const Point& origin, Point& recovery_cell) -> bool {
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
        if (!is_walkable_cell(check_x, check_y)) {
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

auto resolve_walkable_direct_target(const QVector3D& target) -> QVector3D {
  if (CommandService::is_world_position_walkable(target)) {
    return target;
  }

  Point const target_grid = CommandService::world_to_grid(target.x(), target.z());
  auto const nearest = CommandService::find_nearest_walkable_grid(target_grid, 64);
  if (!nearest.has_value()) {
    return target;
  }
  return CommandService::grid_to_world(*nearest);
}

auto should_include_resolved_start_waypoint(const Point& start) -> bool {
  return !CommandService::is_grid_walkable(start);
}

auto segment_traverses_bridge(const QVector3D& from, const QVector3D& to) -> bool {
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
    if (terrain.is_on_bridge(point.x(), point.z())) {
      return true;
    }
  }
  return false;
}

auto align_bridge_waypoint(const QVector3D& waypoint,
                           bool final_waypoint) -> QVector3D {
  if (final_waypoint) {
    return waypoint;
  }
  auto const aligned =
      Game::Map::TerrainService::instance().get_bridge_traversal_position(waypoint.x(),
                                                                          waypoint.z());
  if (!aligned.has_value()) {
    return waypoint;
  }
  return QVector3D(aligned->x(), waypoint.y(), aligned->z());
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
        align_bridge_waypoint(raw_waypoint, idx + 1U == path_points.size());
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
  if (pathfinder == nullptr) {
    assign_direct_target(movement, resolve_walkable_direct_target(requested_target));
    return;
  }

  Point const start =
      CommandService::world_to_grid(transform.position.x, transform.position.z);
  Point const end =
      CommandService::world_to_grid(requested_target.x(), requested_target.z());
  QVector3D const current_pos(transform.position.x, 0.0F, transform.position.z);

  bool const bridge_route = segment_traverses_bridge(current_pos, requested_target);
  if (start == end ||
      (is_direct_path_walkable(current_pos, requested_target) && !bridge_route)) {
    assign_direct_target(movement, resolve_walkable_direct_target(requested_target));
    return;
  }

  auto const path = pathfinder->find_path(start, end);
  bool const include_first_waypoint = should_include_resolved_start_waypoint(start);
  if (!assign_path_to_movement(
          *pathfinder, path, transform, movement, include_first_waypoint)) {
    QVector3D const fallback =
        path.empty() ? resolve_walkable_direct_target(requested_target)
                     : pathfinder->path_waypoint_world_position(path.back());
    assign_direct_target(movement, fallback);
  }
}

auto MovementSystem::assign_local_recovery_move(
    const QVector3D& current_position,
    const QVector3D& goal,
    Engine::Core::MovementComponent* movement) -> bool {
  auto* pathfinder = CommandService::get_pathfinder();
  if (pathfinder == nullptr || movement == nullptr) {
    return false;
  }

  Point const current_grid =
      CommandService::world_to_grid(current_position.x(), current_position.z());

  Point recovery_cell{};
  if (!find_recovery_cell(current_grid, recovery_cell)) {

    constexpr int k_emergency_search_radius = 64;
    auto const nearest = CommandService::find_nearest_walkable_grid(
        current_grid, k_emergency_search_radius);
    if (!nearest.has_value()) {
      return false;
    }
    recovery_cell = *nearest;
  }

  QVector3D const safe_pos = CommandService::grid_to_world(recovery_cell);
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
    Point const desired_goal = CommandService::world_to_grid(goal.x(), goal.z());
    auto const route = pathfinder->find_path(recovery_cell, desired_goal);
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

  assign_navigation_target(
      CommandService::get_pathfinder(), *transform, *movement, goal);
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
  auto* e = world.get_entity(unit_id);
  if (e == nullptr) {
    return;
  }

  auto* atk = e->get_component<Engine::Core::AttackComponent>();
  if ((atk != nullptr) && atk->in_melee_lock &&
      CombatRules::participates_in_rts_melee_lock(e)) {
    auto* locked_target = world.get_entity(atk->melee_lock_target_id);
    auto const* locked_unit =
        locked_target != nullptr
            ? locked_target->get_component<Engine::Core::UnitComponent>()
            : nullptr;
    bool const locked_opponent_alive =
        locked_unit != nullptr && locked_unit->health > 0 &&
        !locked_target->has_component<Engine::Core::PendingRemovalComponent>();
    if (locked_opponent_alive) {
      return;
    }
    CombatRules::clear_rts_melee_lock(e);
  }

  OrderService::prepare_for_move(e, options.kind, options.preserve_formation_mode);

  auto* transform = e->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  auto* mv = e->get_component<Engine::Core::MovementComponent>();
  if (mv == nullptr) {
    mv = e->add_component<Engine::Core::MovementComponent>();
  }
  if (mv == nullptr) {
    return;
  }

  bool const continuous_attack_chase =
      options.kind == MoveOrderKind::AttackChase && mv->get_has_target();
  float const previous_vx = mv->get_vx();
  float const previous_vz = mv->get_vz();
  mv->precise_arrival = options.kind == MoveOrderKind::AttackChase;
  assign_navigation_target(CommandService::get_pathfinder(), *transform, *mv, target);
  if (continuous_attack_chase && mv->get_has_target()) {
    mv->vx = previous_vx;
    mv->vz = previous_vz;
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

  for (std::size_t i = 0; i < units.size(); ++i) {
    issue_move(world, units[i], targets[i], options);
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
