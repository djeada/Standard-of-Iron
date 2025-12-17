#include "command_controller.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/command_service.h"
#include "../../game/systems/formation_planner.h"
#include "../../game/systems/picking_service.h"
#include "../../game/systems/production_service.h"
#include "../../game/systems/selection_system.h"
#include "../../render/gl/camera.h"
#include "../utils/movement_utils.h"
#include "game/game_config.h"
#include "units/spawn_type.h"
#include <QPointF>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

namespace App::Controllers {

CommandController::CommandController(
    Engine::Core::World *world,
    Game::Systems::SelectionSystem *selection_system,
    Game::Systems::PickingService *picking_service, QObject *parent)
    : QObject(parent), m_world(world), m_selection_system(selection_system),
      m_picking_service(picking_service) {}

auto CommandController::on_attack_click(qreal sx, qreal sy, int viewport_width,
                                        int viewport_height,
                                        void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID const target_id =
      Game::Systems::PickingService::pick_unit_first(
          float(sx), float(sy), *m_world, *cam, viewport_width, viewport_height,
          0);

  if (target_id == 0) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto *target_entity = m_world->get_entity(target_id);
  if (target_entity == nullptr) {
    return result;
  }

  auto *target_unit =
      target_entity->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr) {
    return result;
  }

  Game::Systems::CommandService::attack_target(*m_world, selected, target_id,
                                               true);

  emit attack_target_selected();

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_stop_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    reset_movement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();

    if (auto *patrol = entity->get_component<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
      hold_mode->exit_cooldown = hold_mode->stand_up_duration;
      emit hold_mode_changed(false);
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_mode->active = false;
      emit formation_mode_changed(false);
    }
  }

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_hold_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  int eligible_count = 0;
  int hold_active_count = 0;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    eligible_count++;

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_active_count++;
    }
  }

  if (eligible_count == 0) {
    return result;
  }

  const bool should_enable_hold = (hold_active_count < eligible_count);

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();

    if (should_enable_hold) {

      reset_movement(entity);
      entity->remove_component<Engine::Core::AttackTargetComponent>();

      if (auto *patrol =
              entity->get_component<Engine::Core::PatrolComponent>()) {
        patrol->patrolling = false;
        patrol->waypoints.clear();
      }

      if (hold_mode == nullptr) {
        hold_mode = entity->add_component<Engine::Core::HoldModeComponent>();
      }
      hold_mode->active = true;
      hold_mode->exit_cooldown = 0.0F;

      auto *movement = entity->get_component<Engine::Core::MovementComponent>();
      if (movement != nullptr) {
        movement->has_target = false;
        movement->path.clear();
        movement->path_pending = false;
        movement->vx = 0.0F;
        movement->vz = 0.0F;
      }
    } else {

      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
        hold_mode->exit_cooldown = hold_mode->stand_up_duration;
      }
    }
  }

  emit hold_mode_changed(should_enable_hold);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_patrol_click(qreal sx, qreal sy, int viewport_width,
                                        int viewport_height,
                                        void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr) ||
      (m_picking_service == nullptr) || (camera == nullptr)) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    if (m_has_patrol_first_waypoint) {
      clear_patrol_first_waypoint();
      result.reset_cursor_to_normal = true;
    }
    return result;
  }

  if (!m_has_patrol_first_waypoint) {
    m_has_patrol_first_waypoint = true;
    m_patrol_first_waypoint = hit;
    result.input_consumed = true;
    return result;
  }

  QVector3D const second_waypoint = hit;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *building = entity->get_component<Engine::Core::BuildingComponent>();
    if (building != nullptr) {
      continue;
    }

    auto *patrol = entity->get_component<Engine::Core::PatrolComponent>();
    if (patrol == nullptr) {
      patrol = entity->add_component<Engine::Core::PatrolComponent>();
    }

    if (patrol != nullptr) {
      patrol->waypoints.clear();
      patrol->waypoints.emplace_back(m_patrol_first_waypoint.x(),
                                     m_patrol_first_waypoint.z());
      patrol->waypoints.emplace_back(second_waypoint.x(), second_waypoint.z());
      patrol->current_waypoint = 0;
      patrol->patrolling = true;
    }

    reset_movement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();
  }

  clear_patrol_first_waypoint();
  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::set_rally_at_screen(
    qreal sx, qreal sy, int viewport_width, int viewport_height, void *camera,
    int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_world == nullptr) || (m_selection_system == nullptr) ||
      (m_picking_service == nullptr) || (camera == nullptr)) {
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    return result;
  }

  Game::Systems::ProductionService::setRallyForFirstSelectedBarracks(
      *m_world, m_selection_system->get_selected_units(), local_owner_id,
      hit.x(), hit.z());

  result.input_consumed = true;
  return result;
}

void CommandController::recruit_near_selected(const QString &unit_type,
                                              int local_owner_id) {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return;
  }

  const auto &sel = m_selection_system->get_selected_units();
  if (sel.empty()) {
    return;
  }

  auto result =
      Game::Systems::ProductionService::startProductionForFirstSelectedBarracks(
          *m_world, sel, local_owner_id, unit_type.toStdString());

  if (result == Game::Systems::ProductionResult::GlobalTroopLimitReached) {
    emit troop_limit_reached();
  }
}

void CommandController::reset_movement(Engine::Core::Entity *entity) {
  App::Utils::reset_movement(entity);
}

auto CommandController::any_selected_in_hold_mode() const -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto &selected = m_selection_system->get_selected_units();
  for (Engine::Core::EntityID const entity_id : selected) {
    Engine::Core::Entity *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      return true;
    }
  }

  return false;
}

auto CommandController::any_selected_in_guard_mode() const -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto &selected = m_selection_system->get_selected_units();
  for (Engine::Core::EntityID const entity_id : selected) {
    Engine::Core::Entity *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active) {
      return true;
    }
  }

  return false;
}

auto CommandController::on_guard_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  int eligible_count = 0;
  int guard_active_count = 0;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    eligible_count++;

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    if ((guard_mode != nullptr) && guard_mode->active) {
      guard_active_count++;
    }
  }

  if (eligible_count == 0) {
    return result;
  }

  const bool should_enable_guard = (guard_active_count < eligible_count);

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();

    if (should_enable_guard) {

      if (guard_mode == nullptr) {
        guard_mode = entity->add_component<Engine::Core::GuardModeComponent>();
      }
      guard_mode->active = true;
      guard_mode->returning_to_guard_position = false;

      auto *transform =
          entity->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        guard_mode->guard_position_x = transform->position.x;
        guard_mode->guard_position_z = transform->position.z;
        guard_mode->has_guard_target = true;
        guard_mode->guarded_entity_id = 0;
      }

      auto *hold_mode =
          entity->get_component<Engine::Core::HoldModeComponent>();
      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
      }

      if (auto *patrol =
              entity->get_component<Engine::Core::PatrolComponent>()) {
        patrol->patrolling = false;
        patrol->waypoints.clear();
      }
    } else {

      if ((guard_mode != nullptr) && guard_mode->active) {
        guard_mode->active = false;
        guard_mode->guarded_entity_id = 0;
        guard_mode->guard_position_x = 0.0F;
        guard_mode->guard_position_z = 0.0F;
        guard_mode->returning_to_guard_position = false;
        guard_mode->has_guard_target = false;
      }
    }
  }

  emit guard_mode_changed(should_enable_guard);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_guard_click(qreal sx, qreal sy, int viewport_width,
                                       int viewport_height,
                                       void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewport_width, viewport_height, hit)) {
    result.reset_cursor_to_normal = true;
    return result;
  }

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *building = entity->get_component<Engine::Core::BuildingComponent>();
    if (building != nullptr) {
      continue;
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();
    if (guard_mode == nullptr) {
      guard_mode = entity->add_component<Engine::Core::GuardModeComponent>();
    }

    guard_mode->active = true;
    guard_mode->guarded_entity_id = 0;
    guard_mode->guard_position_x = hit.x();
    guard_mode->guard_position_z = hit.z();
    guard_mode->returning_to_guard_position = false;
    guard_mode->has_guard_target = true;

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
    }

    if (auto *patrol = entity->get_component<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    reset_movement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();
  }

  emit guard_mode_changed(true);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

auto CommandController::on_formation_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.size() <= 1) {
    return result;
  }

  int eligible_count = 0;
  int formation_active_count = 0;

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    eligible_count++;

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_active_count++;
    }
  }

  if (eligible_count <= 1) {
    return result;
  }

  const bool should_enable_formation =
      (formation_active_count < eligible_count);

  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();

    if (should_enable_formation) {

      if (formation_mode == nullptr) {
        formation_mode =
            entity->add_component<Engine::Core::FormationModeComponent>();
      }
      formation_mode->active = true;

      auto *hold_mode =
          entity->get_component<Engine::Core::HoldModeComponent>();
      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
      }

      auto *guard_mode =
          entity->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active) {
        guard_mode->active = false;
      }

      if (auto *patrol =
              entity->get_component<Engine::Core::PatrolComponent>()) {
        patrol->patrolling = false;
        patrol->waypoints.clear();
      }
    } else {

      if ((formation_mode != nullptr) && formation_mode->active) {
        formation_mode->active = false;
      }
    }
  }

  if (should_enable_formation) {
    QVector3D center(0.0F, 0.0F, 0.0F);
    int valid_count = 0;

    for (auto id : selected) {
      auto *entity = m_world->get_entity(id);
      if (entity == nullptr) {
        continue;
      }

      auto *transform =
          entity->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        center.setX(center.x() + transform->position.x);
        center.setY(center.y() + transform->position.y);
        center.setZ(center.z() + transform->position.z);
        valid_count++;
      }
    }

    if (valid_count > 0) {
      center.setX(center.x() / static_cast<float>(valid_count));
      center.setY(center.y() / static_cast<float>(valid_count));
      center.setZ(center.z() / static_cast<float>(valid_count));

      auto targets =
          Game::Systems::FormationPlanner::spread_formation_by_nation(
              *m_world, selected, center,
              Game::GameConfig::instance()
                  .gameplay()
                  .formation_spacing_default);

      Game::Systems::CommandService::MoveOptions opts;
      opts.group_move = selected.size() > 1;
      opts.clear_attack_intent = true;
      Game::Systems::CommandService::moveUnits(*m_world, selected, targets,
                                               opts);
    }
  }

  emit formation_mode_changed(should_enable_formation);

  result.input_consumed = true;
  result.reset_cursor_to_normal = true;
  return result;
}

bool CommandController::any_selected_in_formation_mode() const {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto &selected = m_selection_system->get_selected_units();
  for (auto id : selected) {
    auto *entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      return true;
    }
  }

  return false;
}

} // namespace App::Controllers
