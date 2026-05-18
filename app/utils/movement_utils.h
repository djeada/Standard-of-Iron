#pragma once

#include <QPointF>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/order_service.h"
#include "game/systems/pathfinding.h"
#include "game/systems/picking_service.h"
#include "render/gl/camera.h"

namespace App::Utils {

inline void reset_movement(Engine::Core::Entity* entity) {
  Game::Systems::OrderService::reset_movement(entity);
}

inline auto snap_to_walkable_ground(const QVector3D& world_position) -> QVector3D {
  QVector3D snapped = world_position;
  auto& terrain_service = Game::Map::TerrainService::instance();
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  if (pathfinder == nullptr && !terrain_service.is_initialized()) {
    return snapped;
  }

  Game::Systems::Point const grid =
      Game::Systems::CommandService::world_to_grid(snapped.x(), snapped.z());
  auto const is_walkable = [&](int x, int z) {
    if (terrain_service.is_initialized()) {
      return terrain_service.is_walkable(x, z);
    }
    return pathfinder != nullptr && pathfinder->is_walkable(x, z);
  };
  if (is_walkable(grid.x, grid.y)) {
    return snapped;
  }

  Game::Systems::Point nearest = grid;
  bool found = false;
  for (int radius = 1; radius <= 24 && !found; ++radius) {
    for (int dz = -radius; dz <= radius && !found; ++dz) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dz) != radius) {
          continue;
        }
        int const check_x = grid.x + dx;
        int const check_z = grid.y + dz;
        if (!is_walkable(check_x, check_z)) {
          continue;
        }
        nearest = {check_x, check_z};
        found = true;
        break;
      }
    }
  }

  QVector3D const nearest_world =
      Game::Systems::CommandService::grid_to_world(found ? nearest : grid);
  snapped.setX(nearest_world.x());
  snapped.setZ(nearest_world.z());
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));
  return snapped;
}

inline void clear_civilian_delivery_command(Engine::Core::Entity* entity) {
  Game::Systems::OrderService::clear_civilian_delivery(entity);
}

inline void clear_patrol_command(Engine::Core::Entity* entity) {
  Game::Systems::OrderService::clear_patrol(entity);
}

inline auto snap_to_walkable_ground_for_unit(const QVector3D& world_position,
                                             float unit_radius) -> QVector3D {
  QVector3D snapped = world_position;
  auto& terrain_service = Game::Map::TerrainService::instance();
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  if (pathfinder == nullptr && !terrain_service.is_initialized()) {
    return snapped;
  }

  auto const is_walkable = [&](int x, int z) {
    bool path_walkable = true;
    if (pathfinder != nullptr) {
      path_walkable = unit_radius > 0.5F
                          ? pathfinder->is_walkable_with_radius(x, z, unit_radius)
                          : pathfinder->is_walkable(x, z);
    }
    if (!path_walkable) {
      return false;
    }
    if (terrain_service.is_initialized()) {
      return terrain_service.is_walkable(x, z);
    }
    return true;
  };

  Game::Systems::Point const grid =
      Game::Systems::CommandService::world_to_grid(snapped.x(), snapped.z());
  if (is_walkable(grid.x, grid.y)) {
    return snapped;
  }

  Game::Systems::Point nearest = grid;
  bool found = false;
  for (int radius = 1; radius <= 24 && !found; ++radius) {
    for (int dz = -radius; dz <= radius && !found; ++dz) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dz) != radius) {
          continue;
        }
        int const check_x = grid.x + dx;
        int const check_z = grid.y + dz;
        if (!is_walkable(check_x, check_z)) {
          continue;
        }
        nearest = {check_x, check_z};
        found = true;
        break;
      }
    }
  }

  QVector3D const nearest_world =
      Game::Systems::CommandService::grid_to_world(found ? nearest : grid);
  snapped.setX(nearest_world.x());
  snapped.setZ(nearest_world.z());
  snapped.setY(terrain_service.resolve_surface_world_y(
      snapped.x(), snapped.z(), 0.0F, snapped.y()));
  return snapped;
}

inline auto barracks_delivery_target_position(const QVector3D& civilian_position,
                                              const QVector3D& barracks_position,
                                              float unit_radius) -> QVector3D {
  auto const size =
      Game::Systems::BuildingCollisionRegistry::get_building_size("barracks");
  float dir_x = civilian_position.x() - barracks_position.x();
  float dir_z = civilian_position.z() - barracks_position.z();
  float const len_sq = dir_x * dir_x + dir_z * dir_z;
  if (len_sq < 0.0001F) {
    dir_x = 1.0F;
    dir_z = 0.0F;
  } else {
    float const len = std::sqrt(len_sq);
    dir_x /= len;
    dir_z /= len;
  }

  float const clearance = Game::Systems::BuildingCollisionRegistry::get_grid_padding() +
                          unit_radius + 0.25F;
  float const half_width = size.width * 0.5F;
  float const half_depth = size.depth * 0.5F;
  float const abs_x = std::fabs(dir_x);
  float const abs_z = std::fabs(dir_z);
  float const sx = abs_x > 0.0001F ? (half_width + clearance) / abs_x
                                   : std::numeric_limits<float>::infinity();
  float const sz = abs_z > 0.0001F ? (half_depth + clearance) / abs_z
                                   : std::numeric_limits<float>::infinity();
  float const scale = std::min(sx, sz);
  float const fallback_scale = std::max(half_width, half_depth) + clearance;
  float const final_scale =
      std::isfinite(scale) && scale > 0.0F ? scale : fallback_scale;

  QVector3D target(barracks_position.x() + dir_x * final_scale,
                   0.0F,
                   barracks_position.z() + dir_z * final_scale);
  return snap_to_walkable_ground_for_unit(target, unit_radius);
}

inline auto issue_civilian_delivery_command(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& selected,
    Game::Systems::PickingService* picking_service,
    Render::GL::Camera* camera,
    qreal sx,
    qreal sy,
    int viewport_width,
    int viewport_height,
    int local_owner_id) -> bool {
  if ((world == nullptr) || selected.empty() || (picking_service == nullptr) ||
      (camera == nullptr) || (viewport_width <= 0) || (viewport_height <= 0)) {
    return false;
  }

  Engine::Core::EntityID const target_id = picking_service->pick_unit_first(
      float(sx), float(sy), *world, *camera, viewport_width, viewport_height, 0);
  auto* target_entity = target_id != 0U ? world->get_entity(target_id) : nullptr;
  auto* target_unit =
      target_entity != nullptr
          ? target_entity->get_component<Engine::Core::UnitComponent>()
          : nullptr;
  auto* target_transform =
      target_entity != nullptr
          ? target_entity->get_component<Engine::Core::TransformComponent>()
          : nullptr;
  auto* target_production =
      target_entity != nullptr
          ? target_entity->get_component<Engine::Core::ProductionComponent>()
          : nullptr;
  if ((target_unit == nullptr) || (target_transform == nullptr) ||
      (target_production == nullptr) ||
      (target_unit->owner_id != local_owner_id) ||
      (target_unit->spawn_type != Game::Units::SpawnType::Barracks)) {
    return false;
  }

  int const free_population =
      std::max(0, target_production->max_units - target_production->manpower_available);
  int remaining_deliveries =
      free_population / Game::Systems::k_civilian_delivery_population_grant;
  if (remaining_deliveries <= 0) {
    return false;
  }

  std::vector<Engine::Core::EntityID> civilian_ids;
  civilian_ids.reserve(std::min<std::size_t>(selected.size(), remaining_deliveries));
  std::vector<QVector3D> targets;
  targets.reserve(civilian_ids.capacity());

  for (const auto selected_id : selected) {
    if (remaining_deliveries <= 0) {
      break;
    }

    auto* selected_entity = world->get_entity(selected_id);
    auto* selected_unit =
        selected_entity != nullptr
            ? selected_entity->get_component<Engine::Core::UnitComponent>()
            : nullptr;
    if ((selected_unit == nullptr) || (selected_unit->owner_id != local_owner_id) ||
        (selected_unit->spawn_type != Game::Units::SpawnType::Civilian)) {
      continue;
    }

    auto* delivery =
        selected_entity->get_component<Engine::Core::CivilianDeliveryComponent>();
    if (delivery == nullptr) {
      delivery =
          selected_entity->add_component<Engine::Core::CivilianDeliveryComponent>();
    }
    if (delivery == nullptr) {
      continue;
    }
    delivery->target_barracks_id = target_id;

    auto* selected_transform =
        selected_entity->get_component<Engine::Core::TransformComponent>();
    float const unit_radius =
        Game::Systems::CommandService::get_unit_radius(*world, selected_id);
    QVector3D const current_position =
        selected_transform != nullptr
            ? QVector3D(selected_transform->position.x,
                        selected_transform->position.y,
                        selected_transform->position.z)
            : QVector3D(target_transform->position.x,
                        0.0F,
                        target_transform->position.z);
    civilian_ids.push_back(selected_id);
    targets.push_back(barracks_delivery_target_position(
        current_position,
        QVector3D(target_transform->position.x, 0.0F, target_transform->position.z),
        unit_radius));
    --remaining_deliveries;
  }

  if (civilian_ids.empty() || targets.size() != civilian_ids.size()) {
    return false;
  }

  Game::Systems::CommandService::MoveOptions opts;
  opts.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  opts.group_move = false;
  Game::Systems::CommandService::move_units(*world, civilian_ids, targets, opts);
  return true;
}

inline void
issue_move_or_attack_command(Engine::Core::World* world,
                             const std::vector<Engine::Core::EntityID>& selected,
                             Game::Systems::PickingService* picking_service,
                             Render::GL::Camera* camera,
                             qreal sx,
                             qreal sy,
                             int viewport_width,
                             int viewport_height,
                             int local_owner_id) {
  if ((world == nullptr) || selected.empty() || (picking_service == nullptr) ||
      (camera == nullptr) || (viewport_width <= 0) || (viewport_height <= 0)) {
    return;
  }

  Engine::Core::EntityID const target_id = picking_service->pick_unit_first(
      float(sx), float(sy), *world, *camera, viewport_width, viewport_height, 0);

  if (target_id != 0U) {
    auto* target_entity = world->get_entity(target_id);
    if (target_entity != nullptr) {
      auto* target_unit = target_entity->get_component<Engine::Core::UnitComponent>();
      if (target_unit != nullptr) {
        for (const auto selected_id : selected) {
          clear_civilian_delivery_command(world->get_entity(selected_id));
        }

        bool const is_enemy = (target_unit->owner_id != local_owner_id);
        bool const is_building =
            target_entity->has_component<Engine::Core::BuildingComponent>();
        if (is_enemy && !is_building) {
          Game::Systems::CommandService::attack_target(
              *world, selected, target_id, true);
          return;
        }
      }
    }
  }

  QVector3D hit;
  if (!picking_service->screen_to_ground(
          QPointF(sx, sy), *camera, viewport_width, viewport_height, hit)) {
    return;
  }
  hit = snap_to_walkable_ground(hit);

  auto formation_result = Game::Systems::FormationPlanner::get_formation_with_facing(
      *world,
      selected,
      hit,
      Game::GameConfig::instance().gameplay().formation_spacing_default);

  for (const auto selected_id : selected) {
    clear_civilian_delivery_command(world->get_entity(selected_id));
    clear_patrol_command(world->get_entity(selected_id));
  }

  for (size_t i = 0; i < selected.size(); ++i) {
    auto* entity = world->get_entity(selected[i]);
    if (entity == nullptr) {
      continue;
    }

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode == nullptr) || !formation_mode->active) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if ((transform != nullptr) && (i < formation_result.facing_angles.size())) {
      transform->desired_yaw = formation_result.facing_angles[i];
      transform->has_desired_yaw = true;
    }
  }

  Game::Systems::CommandService::MoveOptions opts;
  opts.kind = Game::Systems::MoveOrderKind::FormationMove;
  opts.group_move = selected.size() > 1;
  opts.retry_individual_on_group_failure = selected.size() > 1;
  opts.preserve_formation_mode = formation_result.used_tactical_formation;
  Game::Systems::CommandService::move_units(
      *world, selected, formation_result.positions, opts);
}

} // namespace App::Utils
