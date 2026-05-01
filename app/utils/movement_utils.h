#pragma once

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/terrain_service.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/pathfinding.h"
#include "game/systems/picking_service.h"
#include "render/gl/camera.h"

#include <QPointF>
#include <QVector3D>
#include <cmath>
#include <vector>

namespace App::Utils {

inline void reset_movement(Engine::Core::Entity *entity) {
  if (entity == nullptr) {
    return;
  }

  auto *movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    return;
  }

  auto *transform = entity->get_component<Engine::Core::TransformComponent>();
  movement->has_target = false;
  movement->path.clear();
  movement->path_pending = false;
  movement->pending_request_id = 0;
  movement->repath_cooldown = 0.0F;
  if (transform != nullptr) {
    movement->target_x = transform->position.x;
    movement->target_y = transform->position.z;
    movement->goal_x = transform->position.x;
    movement->goal_y = transform->position.z;
  } else {
    movement->target_x = 0.0F;
    movement->target_y = 0.0F;
    movement->goal_x = 0.0F;
    movement->goal_y = 0.0F;
  }
}

inline auto
snap_to_walkable_ground(const QVector3D &world_position) -> QVector3D {
  QVector3D snapped = world_position;
  auto &terrain_service = Game::Map::TerrainService::instance();
  snapped.setY(terrain_service.resolve_surface_world_y(snapped.x(), snapped.z(),
                                                       0.0F, snapped.y()));

  auto *pathfinder = Game::Systems::CommandService::get_pathfinder();
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
  snapped.setY(terrain_service.resolve_surface_world_y(snapped.x(), snapped.z(),
                                                       0.0F, snapped.y()));
  return snapped;
}

inline void issue_move_or_attack_command(
    Engine::Core::World *world,
    const std::vector<Engine::Core::EntityID> &selected,
    Game::Systems::PickingService *picking_service, Render::GL::Camera *camera,
    qreal sx, qreal sy, int viewport_width, int viewport_height,
    int local_owner_id) {
  if ((world == nullptr) || selected.empty() || (picking_service == nullptr) ||
      (camera == nullptr) || (viewport_width <= 0) || (viewport_height <= 0)) {
    return;
  }

  Engine::Core::EntityID const target_id =
      picking_service->pick_unit_first(float(sx), float(sy), *world, *camera,
                                       viewport_width, viewport_height, 0);

  if (target_id != 0U) {
    auto *target_entity = world->get_entity(target_id);
    if (target_entity != nullptr) {
      auto *target_unit =
          target_entity->get_component<Engine::Core::UnitComponent>();
      if (target_unit != nullptr) {
        bool const is_enemy = (target_unit->owner_id != local_owner_id);
        bool const is_building =
            target_entity->has_component<Engine::Core::BuildingComponent>();
        if (is_enemy && !is_building) {
          Game::Systems::CommandService::attack_target(*world, selected,
                                                       target_id, true);
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

  auto formation_result =
      Game::Systems::FormationPlanner::get_formation_with_facing(
          *world, selected, hit,
          Game::GameConfig::instance().gameplay().formation_spacing_default);

  for (size_t i = 0; i < selected.size(); ++i) {
    auto *entity = world->get_entity(selected[i]);
    if (entity == nullptr) {
      continue;
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode == nullptr) || !formation_mode->active) {
      continue;
    }

    auto *transform = entity->get_component<Engine::Core::TransformComponent>();
    if ((transform != nullptr) && (i < formation_result.facing_angles.size())) {
      transform->desired_yaw = formation_result.facing_angles[i];
      transform->has_desired_yaw = true;
    }
  }

  Game::Systems::CommandService::MoveOptions opts;
  opts.group_move = selected.size() > 1;
  Game::Systems::CommandService::move_units(*world, selected,
                                            formation_result.positions, opts);
}

} // namespace App::Utils
