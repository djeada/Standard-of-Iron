#include "command_service.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "movement_system.h"
#include "pathfinding.h"
#include "units/spawn_type.h"

namespace Game::Systems {

namespace {
auto resolve_walkable_target(const QVector3D& target) -> QVector3D {
  return CommandService::snap_to_walkable_ground(target, 8);
}

} // namespace

auto CommandService::resolve_move_targets(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& units,
    const QVector3D& center) -> std::vector<QVector3D> {
  std::vector<QVector3D> targets;
  targets.reserve(units.size());
  if (units.empty()) {
    return targets;
  }

  QVector3D current_center(0.0F, 0.0F, 0.0F);
  int positioned_count = 0;
  float max_radius = CommandService::k_unit_radius_threshold;
  for (auto unit_id : units) {
    float const unit_radius = CommandService::get_unit_radius(world, unit_id);
    max_radius = std::max(max_radius, unit_radius);
    auto* entity = world.get_entity(unit_id);
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      continue;
    }
    current_center += QVector3D(transform->position.x, 0.0F, transform->position.z);
    ++positioned_count;
  }

  if (positioned_count > 0) {
    current_center /= static_cast<float>(positioned_count);
  } else {
    current_center = center - QVector3D(0.0F, 0.0F, 1.0F);
  }

  QVector3D const center_target = resolve_walkable_target(center);
  Point const center_grid =
      CommandService::world_to_grid(center_target.x(), center_target.z());
  if (!CommandService::is_grid_walkable(center_grid)) {
    targets.assign(units.size(), center_target);
    return targets;
  }

  QVector3D forward = center - current_center;
  forward.setY(0.0F);
  if (forward.lengthSquared() <= 1.0e-4F) {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    forward.normalize();
  }

  QVector3D right(forward.z(), 0.0F, -forward.x());
  if (right.lengthSquared() <= 1.0e-4F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right.normalize();
  }

  int const columns = std::max(
      1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(units.size())))));
  float const lane_center = (static_cast<float>(columns) - 1.0F) * 0.5F;
  float const spacing =
      std::max(Game::GameConfig::instance().gameplay().formation_spacing_default,
               max_radius * 2.8F + 0.8F);

  for (std::size_t idx = 0; idx < units.size(); ++idx) {
    int const col = static_cast<int>(idx % static_cast<std::size_t>(columns));
    int const row = static_cast<int>(idx / static_cast<std::size_t>(columns));
    QVector3D const offset =
        right * ((static_cast<float>(col) - lane_center) * spacing) -
        forward * (static_cast<float>(row) * spacing);
    QVector3D const candidate = center + offset;
    Point const candidate_grid =
        CommandService::world_to_grid(candidate.x(), candidate.z());
    if (CommandService::is_grid_walkable(candidate_grid)) {
      targets.push_back(resolve_walkable_target(candidate));
    } else {
      targets.push_back(center_target);
    }
  }

  return targets;
}

std::unique_ptr<Pathfinding> CommandService::s_pathfinder = nullptr;

void CommandService::initialize(int world_width, int world_height) {
  s_pathfinder = std::make_unique<Pathfinding>(world_width, world_height);

  float const offset_x = -(world_width * 0.5F - 0.5F);
  float const offset_z = -(world_height * 0.5F - 0.5F);
  s_pathfinder->set_grid_offset(offset_x, offset_z);
}

auto CommandService::get_pathfinder() -> Pathfinding* {
  return s_pathfinder.get();
}
auto CommandService::world_to_grid(float world_x, float world_z) -> Point {
  if (s_pathfinder) {
    return s_pathfinder->world_to_grid(world_x, world_z);
  }

  return {static_cast<int>(std::round(world_x)), static_cast<int>(std::round(world_z))};
}

auto CommandService::grid_to_world(const Point& grid_pos) -> QVector3D {
  if (s_pathfinder) {
    return s_pathfinder->grid_to_world(grid_pos);
  }
  return {static_cast<float>(grid_pos.x), 0.0F, static_cast<float>(grid_pos.y)};
}

auto CommandService::is_grid_walkable(const Point& grid_pos) -> bool {
  if (s_pathfinder != nullptr) {
    s_pathfinder->update_navigation_grid();
    return s_pathfinder->is_walkable(grid_pos.x, grid_pos.y);
  }

  auto& terrain_service = Game::Map::TerrainService::instance();
  if (terrain_service.is_initialized()) {
    return terrain_service.is_walkable(grid_pos.x, grid_pos.y);
  }

  return true;
}

auto CommandService::is_world_position_walkable(const QVector3D& world_position)
    -> bool {
  Point const grid = world_to_grid(world_position.x(), world_position.z());
  return is_grid_walkable(grid);
}

auto CommandService::find_nearest_walkable_grid(
    const Point& origin, int max_search_radius) -> std::optional<Point> {
  if (max_search_radius < 0) {
    return std::nullopt;
  }

  auto is_candidate_walkable = [&](const Point& candidate) -> bool {
    if (s_pathfinder != nullptr) {
      return s_pathfinder->is_walkable(candidate.x, candidate.y);
    }
    return is_grid_walkable(candidate);
  };

  if (s_pathfinder != nullptr) {
    s_pathfinder->update_navigation_grid();
  }

  if (is_candidate_walkable(origin)) {
    return origin;
  }

  for (int radius = 1; radius <= max_search_radius; ++radius) {
    Point best{};
    int best_distance_sq = std::numeric_limits<int>::max();
    bool found = false;
    for (int dz = -radius; dz <= radius; ++dz) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dz) != radius) {
          continue;
        }
        Point const candidate{origin.x + dx, origin.y + dz};
        if (!is_candidate_walkable(candidate)) {
          continue;
        }

        int const distance_sq = dx * dx + dz * dz;
        if (!found || distance_sq < best_distance_sq) {
          best = candidate;
          best_distance_sq = distance_sq;
        }
        found = true;
      }
    }
    if (found) {
      return best;
    }
  }

  return std::nullopt;
}

auto CommandService::snap_to_walkable_ground(const QVector3D& world_position)
    -> QVector3D {
  return snap_to_walkable_ground(world_position, 24);
}

auto CommandService::snap_to_walkable_ground(const QVector3D& world_position,
                                             int max_search_radius) -> QVector3D {
  QVector3D snapped = world_position;
  auto& terrain_service = Game::Map::TerrainService::instance();
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));

  Point const grid = world_to_grid(snapped.x(), snapped.z());
  auto const nearest = find_nearest_walkable_grid(grid, max_search_radius);
  if (!nearest.has_value()) {
    return snapped;
  }

  QVector3D const nearest_world = grid_to_world(*nearest);
  snapped.setX(nearest_world.x());
  snapped.setZ(nearest_world.z());
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));
  return snapped;
}

auto CommandService::plan_ground_move(Engine::Core::World& world,
                                      const std::vector<Engine::Core::EntityID>& units,
                                      const QVector3D& target) -> GroundMovePlan {
  GroundMovePlan plan;
  if (units.empty()) {
    return plan;
  }

  plan.resolved_target = resolve_walkable_target(target);
  plan.positions = resolve_move_targets(world, units, plan.resolved_target);
  plan.facing_angles.assign(units.size(), 0.0F);
  plan.preserve_formation_mode = false;
  if (units.size() == 1 && !plan.positions.empty()) {
    plan.resolved_target = plan.positions.front();
  }
  return plan;
}

void CommandService::issue_ground_move(Engine::Core::World& world,
                                       const std::vector<Engine::Core::EntityID>& units,
                                       const GroundMovePlan& plan) {
  if (units.empty() || units.size() != plan.positions.size()) {
    return;
  }

  for (std::size_t i = 0; i < units.size(); ++i) {
    auto* entity = world.get_entity(units[i]);
    if (entity == nullptr) {
      continue;
    }

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode == nullptr) || !formation_mode->active ||
        i >= plan.facing_angles.size()) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      transform->desired_yaw = plan.facing_angles[i];
      transform->has_desired_yaw = true;
    }
  }

  MoveOptions opts;
  opts.kind = MoveOrderKind::FormationMove;
  opts.preserve_formation_mode = plan.preserve_formation_mode;
  move_units(world, units, plan.positions, opts);
}

auto CommandService::get_unit_radius(Engine::Core::World& world,
                                     Engine::Core::EntityID entity_id) -> float {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return 0.5F;
  }

  auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_comp == nullptr) {
    return 0.5F;
  }

  float const selection_ring_size =
      Game::Units::TroopConfig::instance().get_selection_ring_size(
          unit_comp->spawn_type);

  return std::max(selection_ring_size * 0.5F, k_unit_radius_threshold);
}

void CommandService::move_unit(Engine::Core::World& world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D& target) {
  MovementSystem::issue_move(world, unit_id, target);
}

void CommandService::move_unit(Engine::Core::World& world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D& target,
                               const MoveOptions& options) {
  MovementSystem::issue_move(world, unit_id, target, options);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const std::vector<QVector3D>& targets) {
  MovementSystem::issue_move_units(world, units, targets);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const std::vector<QVector3D>& targets,
                                const MoveOptions& options) {
  MovementSystem::issue_move_units(world, units, targets, options);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<MoveIntent>& intents) {
  MovementSystem::issue_move_units(world, intents);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<MoveIntent>& intents,
                                const MoveOptions& options) {
  MovementSystem::issue_move_units(world, intents, options);
}

void CommandService::attack_target(Engine::Core::World& world,
                                   const std::vector<Engine::Core::EntityID>& units,
                                   Engine::Core::EntityID target_id,
                                   bool should_chase) {
  if (target_id == 0) {
    return;
  }
  auto* target_ent = world.get_entity(target_id);
  auto* target_transform =
      target_ent != nullptr
          ? target_ent->get_component<Engine::Core::TransformComponent>()
          : nullptr;
  QVector3D const target_pos =
      target_transform != nullptr
          ? QVector3D(target_transform->position.x, 0.0F, target_transform->position.z)
          : QVector3D();
  float target_radius =
      target_ent != nullptr ? std::max(get_unit_radius(world, target_id), 0.5F) : 0.5F;
  if (target_ent != nullptr && target_transform != nullptr &&
      target_ent->has_component<Engine::Core::BuildingComponent>()) {
    target_radius =
        std::max(target_radius,
                 std::max(target_transform->scale.x, target_transform->scale.z) * 0.5F);
  }

  QVector3D attack_center = target_pos;
  if (should_chase && target_transform != nullptr && !units.empty()) {
    QVector3D current_center(0.0F, 0.0F, 0.0F);
    int positioned_count = 0;
    float max_attacker_radius = k_unit_radius_threshold;
    float max_range = 2.0F;
    bool has_ranged_unit = false;
    for (auto unit_id : units) {
      max_attacker_radius =
          std::max(max_attacker_radius, get_unit_radius(world, unit_id));
      auto* attacker = world.get_entity(unit_id);
      auto* attacker_transform =
          attacker != nullptr
              ? attacker->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      if (attacker_transform != nullptr) {
        current_center += QVector3D(
            attacker_transform->position.x, 0.0F, attacker_transform->position.z);
        ++positioned_count;
      }
      if (attacker != nullptr) {
        if (auto* atk = attacker->get_component<Engine::Core::AttackComponent>()) {
          max_range = std::max(max_range, std::max(0.1F, atk->range));
          has_ranged_unit = has_ranged_unit ||
                            (atk->can_ranged && atk->range > atk->melee_range * 1.5F);
        }
      }
    }

    if (positioned_count > 0) {
      current_center /= static_cast<float>(positioned_count);
    } else {
      current_center = target_pos - QVector3D(1.0F, 0.0F, 0.0F);
    }

    QVector3D away_from_target = current_center - target_pos;
    away_from_target.setY(0.0F);
    if (away_from_target.lengthSquared() <= 1.0e-4F) {
      away_from_target = QVector3D(1.0F, 0.0F, 0.0F);
    } else {
      away_from_target.normalize();
    }

    float desired_distance =
        target_radius + max_attacker_radius + std::max(max_range - 0.2F, 0.2F);
    if (has_ranged_unit) {
      desired_distance = target_radius + max_attacker_radius + max_range * 0.85F;
    }
    attack_center = target_pos + away_from_target * desired_distance;
  }
  std::vector<QVector3D> attack_grid =
      should_chase ? resolve_move_targets(world, units, attack_center)
                   : std::vector<QVector3D>{};

  for (std::size_t index = 0; index < units.size(); ++index) {
    auto unit_id = units[index];
    auto* e = world.get_entity(unit_id);
    if (e == nullptr) {
      continue;
    }

    OrderService::prepare_for_attack(e);

    auto* attack_target = e->get_component<Engine::Core::AttackTargetComponent>();
    if (attack_target == nullptr) {
      attack_target = e->add_component<Engine::Core::AttackTargetComponent>();
    }
    if (attack_target == nullptr) {
      continue;
    }

    attack_target->target_id = target_id;
    attack_target->should_chase = should_chase;

    if (!should_chase) {
      continue;
    }

    auto* att_trans = e->get_component<Engine::Core::TransformComponent>();
    if ((target_transform == nullptr) || (att_trans == nullptr)) {
      continue;
    }

    QVector3D const desired_pos =
        (index < attack_grid.size()) ? attack_grid[index] : attack_center;

    CommandService::MoveOptions opts;
    opts.kind = MoveOrderKind::AttackChase;
    CommandService::move_unit(world, unit_id, desired_pos, opts);
  }
}

} // namespace Game::Systems
