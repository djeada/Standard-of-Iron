#pragma once

#include <QPointF>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "../core/rts_action_model.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/command_service.h"
#include "game/systems/order_service.h"
#include "game/systems/pathfinding.h"
#include "game/systems/picking_service.h"
#include "render/gl/camera.h"

namespace App::Utils {

inline void reset_movement(Engine::Core::Entity* entity) {
  Game::Systems::OrderService::reset_movement(entity);
}

inline auto snap_to_walkable_ground(const QVector3D& world_position) -> QVector3D {
  return Game::Systems::CommandService::snap_to_walkable_ground(world_position);
}

inline void clear_civilian_delivery_command(Engine::Core::Entity* entity) {
  Game::Systems::OrderService::clear_civilian_delivery(entity);
}

inline void clear_patrol_command(Engine::Core::Entity* entity) {
  Game::Systems::OrderService::clear_patrol(entity);
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
  return snap_to_walkable_ground(target);
}

inline auto
issue_civilian_delivery_command(Engine::Core::World* world,
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
  auto* target_unit = target_entity != nullptr
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
      (target_production == nullptr) || (target_unit->owner_id != local_owner_id) ||
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
    QVector3D const current_position = selected_transform != nullptr
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
          auto const attackers = App::Core::filter_selected_units_for_action(
              world, selected, QStringLiteral("attack"));
          if (attackers.empty()) {
            return;
          }
          Game::Systems::CommandService::attack_target(
              *world, attackers, target_id, true);
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
  auto const plan =
      Game::Systems::CommandService::plan_ground_move(*world, selected, hit);
  Game::Systems::CommandService::issue_ground_move(*world, selected, plan);
}

} // namespace App::Utils
