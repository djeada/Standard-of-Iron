#include "movement_utils.h"

#include <algorithm>
#include <string>

#include "../core/rts_action_model.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/command_service.h"
#include "game/systems/nav_grid.h"
#include "game/systems/picking_service.h"
#include "game/units/spawn_type.h"
#include "scene/camera.h"

namespace App::Utils {

auto snap_to_walkable_ground(const QVector3D& world_position) -> QVector3D {
  return Game::Systems::NavGrid::snap_to_walkable_ground(world_position);
}

auto issue_civilian_delivery_command(
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
  auto* target_unit = target_entity != nullptr
                          ? target_entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  auto* target_production =
      target_entity != nullptr
          ? target_entity->get_component<Engine::Core::ProductionComponent>()
          : nullptr;
  if ((target_unit == nullptr) || (target_production == nullptr) ||
      (target_unit->owner_id != local_owner_id) ||
      (target_unit->spawn_type != Game::Units::SpawnType::Barracks)) {
    return false;
  }

  int const free_population =
      std::max(0, target_production->max_units - target_production->manpower_available);
  if (free_population / Game::Systems::k_civilian_delivery_population_grant <= 0) {
    return false;
  }

  std::vector<Engine::Core::EntityID> civilians;
  for (const auto selected_id : selected) {
    auto* entity = world->get_entity(selected_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if ((unit != nullptr) && (unit->owner_id == local_owner_id) &&
        (unit->spawn_type == Game::Units::SpawnType::Civilian)) {
      civilians.push_back(selected_id);
    }
  }
  if (civilians.empty()) {
    return false;
  }

  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        local_owner_id,
                        Game::Command::DeliverCivilians{.units = std::move(civilians),
                                                        .barracks = target_id});
  return true;
}

auto issue_builder_repair_command(Engine::Core::World* world,
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
  if (target_entity == nullptr ||
      !target_entity->has_component<Engine::Core::BuildingComponent>()) {
    return false;
  }
  auto* target_unit = target_entity->get_component<Engine::Core::UnitComponent>();
  if ((target_unit == nullptr) || (target_unit->owner_id != local_owner_id) ||
      target_unit->health <= 0 || target_unit->health >= target_unit->max_health) {
    return false;
  }

  std::vector<Engine::Core::EntityID> builders;
  for (const auto selected_id : selected) {
    auto* entity = world->get_entity(selected_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if ((unit != nullptr) && (unit->owner_id == local_owner_id) &&
        (unit->spawn_type == Game::Units::SpawnType::Builder) &&
        entity->has_component<Engine::Core::BuilderProductionComponent>()) {
      builders.push_back(selected_id);
    }
  }
  if (builders.empty()) {
    return false;
  }

  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        local_owner_id,
                        Game::Command::RepairStructure{.units = std::move(builders),
                                                       .structure = target_id});
  return true;
}

void submit_ground_move(Engine::Core::World& world,
                        const std::vector<Engine::Core::EntityID>& units,
                        const QVector3D& destination,
                        int owner_id) {
  const auto plan =
      Game::Systems::CommandService::plan_ground_move(world, units, destination);
  if (units.size() != plan.positions.size()) {
    return;
  }

  Game::Command::Move move;
  move.units = units;
  move.targets.assign(plan.positions.begin(), plan.positions.end());
  move.facing_angles = plan.facing_angles;
  move.kind = Game::Systems::MoveOrderKind::FormationMove;
  move.preserve_formation_mode = plan.preserve_formation_mode;
  Game::Command::submit(
      world, Game::Command::Source::LocalPlayer, owner_id, std::move(move));
}

void issue_move_or_attack_command(Engine::Core::World* world,
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
        bool const is_enemy = (target_unit->owner_id != local_owner_id);
        bool const is_building =
            target_entity->has_component<Engine::Core::BuildingComponent>();
        if (is_enemy && !is_building) {
          auto const attackers = App::Core::filter_selected_units_for_action(
              world, selected, QStringLiteral("attack"));
          if (attackers.empty()) {
            return;
          }
          Game::Command::submit(
              *world,
              Game::Command::Source::LocalPlayer,
              local_owner_id,
              Game::Command::AttackTarget{.units = attackers, .target = target_id});
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
  submit_ground_move(*world, selected, hit, local_owner_id);
}

} // namespace App::Utils
