#include "app/orders/movement_utils.h"

#include <algorithm>
#include <string>
#include <utility>

#include "app/orders/order_submission.h"
#include "app/orders/rts_action_model.h"
#include "game/command/command.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/command_service.h"
#include "game/systems/construction_cost_catalog.h"
#include "game/systems/nav_grid.h"
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
    int local_owner_id) -> App::Core::OrderOutcome {
  using App::Core::OrderKind;
  if ((world == nullptr) || selected.empty() || (picking_service == nullptr) ||
      (camera == nullptr) || (viewport_width <= 0) || (viewport_height <= 0)) {
    return App::Core::rejected_order(OrderKind::Deliver,
                                     App::Core::no_selection_reason());
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
      !Game::Units::is_recruitment_building(target_unit->spawn_type)) {
    return App::Core::rejected_order_on(
        OrderKind::Deliver,
        App::Core::no_target_under_cursor_reason(OrderKind::Deliver),
        target_unit != nullptr ? target_id : 0);
  }

  int const free_population =
      std::max(0, target_production->max_units - target_production->manpower_available);
  if (free_population / Game::Systems::k_civilian_delivery_population_grant <= 0) {
    return App::Core::rejected_order_on(
        OrderKind::Deliver, App::Core::barracks_full_reason(), target_id);
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
    return App::Core::rejected_order_on(
        OrderKind::Deliver,
        App::Core::no_eligible_units_reason(OrderKind::Deliver),
        target_id);
  }

  App::Core::OrderRequest request;
  request.kind = OrderKind::Deliver;
  request.payload = Game::Command::DeliverCivilians{.units = std::move(civilians),
                                                    .barracks = target_id};
  request.target = target_id;
  return App::Core::submit_player_order(*world, local_owner_id, std::move(request));
}

auto issue_builder_repair_command(Engine::Core::World* world,
                                  const std::vector<Engine::Core::EntityID>& selected,
                                  Game::Systems::PickingService* picking_service,
                                  Render::GL::Camera* camera,
                                  qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  int local_owner_id) -> App::Core::OrderOutcome {
  using App::Core::OrderKind;
  if ((world == nullptr) || selected.empty() || (picking_service == nullptr) ||
      (camera == nullptr) || (viewport_width <= 0) || (viewport_height <= 0)) {
    return App::Core::rejected_order(OrderKind::Repair,
                                     App::Core::no_selection_reason());
  }

  Engine::Core::EntityID const target_id = picking_service->pick_unit_first(
      float(sx), float(sy), *world, *camera, viewport_width, viewport_height, 0);
  auto* target_entity = target_id != 0U ? world->get_entity(target_id) : nullptr;
  if (target_entity == nullptr ||
      !target_entity->has_component<Engine::Core::BuildingComponent>()) {
    return App::Core::rejected_order(
        OrderKind::Repair, App::Core::no_target_under_cursor_reason(OrderKind::Repair));
  }
  auto* target_unit = target_entity->get_component<Engine::Core::UnitComponent>();
  if ((target_unit == nullptr) || (target_unit->owner_id != local_owner_id) ||
      target_unit->health <= 0) {
    return App::Core::rejected_order_on(
        OrderKind::Repair,
        App::Core::no_target_under_cursor_reason(OrderKind::Repair),
        target_id);
  }
  if (target_unit->health >= target_unit->max_health) {
    return App::Core::rejected_order_on(
        OrderKind::Repair, App::Core::no_repairs_needed_reason(), target_id);
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
    return App::Core::rejected_order_on(
        OrderKind::Repair,
        App::Core::no_eligible_units_reason(OrderKind::Repair),
        target_id);
  }

  App::Core::OrderRequest request;
  request.kind = OrderKind::Repair;
  request.payload = Game::Command::RepairStructure{.units = std::move(builders),
                                                   .structure = target_id};
  request.target = target_id;
  return App::Core::submit_player_order(*world, local_owner_id, std::move(request));
}

auto issue_builder_dismantle_command(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& selected,
    Game::Systems::PickingService* picking_service,
    Render::GL::Camera* camera,
    qreal sx,
    qreal sy,
    int viewport_width,
    int viewport_height,
    int local_owner_id) -> App::Core::OrderOutcome {
  using App::Core::OrderKind;
  if ((world == nullptr) || selected.empty() || (picking_service == nullptr) ||
      (camera == nullptr) || (viewport_width <= 0) || (viewport_height <= 0)) {
    return App::Core::rejected_order(OrderKind::Build,
                                     App::Core::no_selection_reason());
  }

  Engine::Core::EntityID const target_id = picking_service->pick_unit_first(
      float(sx), float(sy), *world, *camera, viewport_width, viewport_height, 0);
  auto* target_entity = target_id != 0U ? world->get_entity(target_id) : nullptr;
  if (target_entity == nullptr ||
      !target_entity->has_component<Engine::Core::BuildingComponent>()) {
    return App::Core::rejected_order(
        OrderKind::Build, App::Core::no_target_under_cursor_reason(OrderKind::Build));
  }
  auto* target_unit = target_entity->get_component<Engine::Core::UnitComponent>();
  if ((target_unit == nullptr) || (target_unit->owner_id != local_owner_id) ||
      target_unit->health <= 0) {
    return App::Core::rejected_order_on(
        OrderKind::Build, App::Core::not_your_building_reason(), target_id);
  }
  if (!Game::Systems::dismantle_info(
           Game::Units::spawn_typeToString(target_unit->spawn_type))
           .allowed) {
    return App::Core::rejected_order_on(
        OrderKind::Build, App::Core::building_is_protected_reason(), target_id);
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
    return App::Core::rejected_order_on(
        OrderKind::Build,
        App::Core::no_eligible_units_reason(OrderKind::Build),
        target_id);
  }

  App::Core::OrderRequest request;
  request.kind = OrderKind::Build;
  request.payload = Game::Command::DismantleStructure{.units = std::move(builders),
                                                      .structure = target_id};
  request.target = target_id;
  return App::Core::submit_player_order(*world, local_owner_id, std::move(request));
}

auto submit_ground_move(Engine::Core::World& world,
                        const std::vector<Engine::Core::EntityID>& units,
                        const QVector3D& destination,
                        int owner_id) -> App::Core::OrderOutcome {
  using App::Core::OrderKind;
  if (units.empty()) {
    return App::Core::rejected_order_at(
        OrderKind::Move, App::Core::no_selection_reason(), destination);
  }

  const auto plan =
      Game::Systems::CommandService::plan_ground_move(world, units, destination);
  if (units.size() != plan.positions.size()) {
    return App::Core::rejected_order_at(
        OrderKind::Move,
        App::Core::rejection_reason_text(Game::Command::Rejection::MalformedPayload,
                                         OrderKind::Move),
        destination);
  }

  Game::Command::Move move;
  move.units = units;
  move.targets.assign(plan.positions.begin(), plan.positions.end());
  move.facing_angles = plan.facing_angles;
  move.kind = Game::Systems::MoveOrderKind::FormationMove;
  move.preserve_formation_mode = plan.preserve_formation_mode;

  App::Core::OrderRequest request;
  request.kind = OrderKind::Move;
  request.payload = std::move(move);
  request.has_destination = true;
  request.destination = destination;
  return App::Core::submit_player_order(world, owner_id, std::move(request));
}

auto pick_enemy_unit_at_screen(Engine::Core::World* world,
                               Render::GL::Camera* camera,
                               qreal sx,
                               qreal sy,
                               int viewport_width,
                               int viewport_height,
                               int local_owner_id) -> Engine::Core::EntityID {
  if ((world == nullptr) || (camera == nullptr) || (viewport_width <= 0) ||
      (viewport_height <= 0)) {
    return 0;
  }
  Engine::Core::EntityID const target_id =
      Game::Systems::PickingService::pick_unit_first(
          float(sx), float(sy), *world, *camera, viewport_width, viewport_height, 0);
  if (target_id == 0U) {
    return 0;
  }
  auto* target_entity = world->get_entity(target_id);
  auto* target_unit = target_entity != nullptr
                          ? target_entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  if (target_unit == nullptr) {
    return 0;
  }
  bool const is_enemy = (target_unit->owner_id != local_owner_id);
  bool const is_building =
      target_entity->has_component<Engine::Core::BuildingComponent>();
  return (is_enemy && !is_building) ? target_id : 0;
}

auto issue_attack_command(Engine::Core::World* world,
                          const std::vector<Engine::Core::EntityID>& selected,
                          Engine::Core::EntityID target_id,
                          int local_owner_id) -> App::Core::OrderOutcome {
  using App::Core::OrderKind;
  if (world == nullptr || selected.empty()) {
    return App::Core::rejected_order_on(
        OrderKind::Attack, App::Core::no_selection_reason(), target_id);
  }
  auto const attackers = App::Core::filter_selected_units_for_action(
      world, selected, QStringLiteral("attack"));
  if (attackers.empty()) {
    return App::Core::rejected_order_on(
        OrderKind::Attack,
        App::Core::no_eligible_units_reason(OrderKind::Attack),
        target_id);
  }
  App::Core::OrderRequest request;
  request.kind = OrderKind::Attack;
  request.payload =
      Game::Command::AttackTarget{.units = attackers, .target = target_id};
  request.target = target_id;
  return App::Core::submit_player_order(*world, local_owner_id, std::move(request));
}

auto issue_move_or_attack_command(Engine::Core::World* world,
                                  const std::vector<Engine::Core::EntityID>& selected,
                                  Game::Systems::PickingService* picking_service,
                                  Render::GL::Camera* camera,
                                  qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  int local_owner_id) -> App::Core::OrderOutcome {
  using App::Core::OrderKind;
  if ((world == nullptr) || selected.empty() || (picking_service == nullptr) ||
      (camera == nullptr) || (viewport_width <= 0) || (viewport_height <= 0)) {
    return App::Core::rejected_order(OrderKind::Move, App::Core::no_selection_reason());
  }

  Engine::Core::EntityID const target_id = pick_enemy_unit_at_screen(
      world, camera, sx, sy, viewport_width, viewport_height, local_owner_id);
  if (target_id != 0U) {
    return issue_attack_command(world, selected, target_id, local_owner_id);
  }

  QVector3D hit;
  if (!picking_service->screen_to_ground(
          QPointF(sx, sy), *camera, viewport_width, viewport_height, hit)) {
    return App::Core::rejected_order(OrderKind::Move,
                                     App::Core::no_ground_under_cursor_reason());
  }
  return submit_ground_move(*world, selected, hit, local_owner_id);
}

} // namespace App::Utils
