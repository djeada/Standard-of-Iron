#include "command_controller.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/command_service.h"
#include "../../game/systems/picking_service.h"
#include "../../game/systems/production_service.h"
#include "../../game/systems/selection_system.h"
#include "../../render/gl/camera.h"
#include "../utils/movement_utils.h"
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
    Game::Systems::PickingService *pickingService, QObject *parent)
    : QObject(parent), m_world(world), m_selection_system(selection_system),
      m_pickingService(pickingService) {}

auto CommandController::onAttackClick(qreal sx, qreal sy, int viewportWidth,
                                      int viewportHeight,
                                      void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_pickingService == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.resetCursorToNormal = true;
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.resetCursorToNormal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID const target_id =
      Game::Systems::PickingService::pick_unit_first(
          float(sx), float(sy), *m_world, *cam, viewportWidth, viewportHeight,
          0);

  if (target_id == 0) {
    result.resetCursorToNormal = true;
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

  emit attack_targetSelected();

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

auto CommandController::onStopCommand() -> CommandResult {
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

    resetMovement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();

    if (auto *patrol = entity->get_component<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
      hold_mode->exit_cooldown = hold_mode->stand_up_duration;
      emit hold_modeChanged(false);
    }
  }

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

auto CommandController::onHoldCommand() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  // First, determine if all eligible units already have hold mode active
  // If all do, we disable it; if any don't, we enable it for all
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

    // Skip buildings
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

  // Determine target state: if all have hold active, disable; otherwise enable
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

    // Skip buildings
    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();

    if (should_enable_hold) {
      // Enable hold mode
      resetMovement(entity);
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
      // Disable hold mode
      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
        hold_mode->exit_cooldown = hold_mode->stand_up_duration;
      }
    }
  }

  emit hold_modeChanged(should_enable_hold);

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

auto CommandController::onPatrolClick(qreal sx, qreal sy, int viewportWidth,
                                      int viewportHeight,
                                      void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr) ||
      (m_pickingService == nullptr) || (camera == nullptr)) {
    if (m_hasPatrolFirstWaypoint) {
      clearPatrolFirstWaypoint();
      result.resetCursorToNormal = true;
    }
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    if (m_hasPatrolFirstWaypoint) {
      clearPatrolFirstWaypoint();
      result.resetCursorToNormal = true;
    }
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewportWidth, viewportHeight, hit)) {
    if (m_hasPatrolFirstWaypoint) {
      clearPatrolFirstWaypoint();
      result.resetCursorToNormal = true;
    }
    return result;
  }

  if (!m_hasPatrolFirstWaypoint) {
    m_hasPatrolFirstWaypoint = true;
    m_patrolFirstWaypoint = hit;
    result.inputConsumed = true;
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
      patrol->waypoints.emplace_back(m_patrolFirstWaypoint.x(),
                                     m_patrolFirstWaypoint.z());
      patrol->waypoints.emplace_back(second_waypoint.x(), second_waypoint.z());
      patrol->current_waypoint = 0;
      patrol->patrolling = true;
    }

    resetMovement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();
  }

  clearPatrolFirstWaypoint();
  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

auto CommandController::setRallyAtScreen(qreal sx, qreal sy, int viewportWidth,
                                         int viewportHeight, void *camera,
                                         int local_owner_id) -> CommandResult {
  CommandResult result;
  if ((m_world == nullptr) || (m_selection_system == nullptr) ||
      (m_pickingService == nullptr) || (camera == nullptr)) {
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewportWidth, viewportHeight, hit)) {
    return result;
  }

  Game::Systems::ProductionService::setRallyForFirstSelectedBarracks(
      *m_world, m_selection_system->get_selected_units(), local_owner_id,
      hit.x(), hit.z());

  result.inputConsumed = true;
  return result;
}

void CommandController::recruitNearSelected(const QString &unit_type,
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
    emit troopLimitReached();
  }
}

void CommandController::resetMovement(Engine::Core::Entity *entity) {
  App::Utils::reset_movement(entity);
}

auto CommandController::anySelectedInHoldMode() const -> bool {
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

auto CommandController::anySelectedInGuardMode() const -> bool {
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

auto CommandController::onGuardCommand() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  // First, determine if all eligible units already have guard mode active
  // If all do, we disable it; if any don't, we enable it for all
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

    // Skip buildings
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

  // Determine target state: if all have guard active, disable; otherwise enable
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

    // Skip buildings
    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto *guard_mode =
        entity->get_component<Engine::Core::GuardModeComponent>();

    if (should_enable_guard) {
      // Enable guard mode at current position
      if (guard_mode == nullptr) {
        guard_mode = entity->add_component<Engine::Core::GuardModeComponent>();
      }
      guard_mode->active = true;
      guard_mode->returning_to_guard_position = false;

      // Set guard position to current position
      auto *transform =
          entity->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        guard_mode->guard_position_x = transform->position.x;
        guard_mode->guard_position_z = transform->position.z;
        guard_mode->has_guard_target = true;
        guard_mode->guarded_entity_id = 0;
      }

      // Disable hold mode if active
      auto *hold_mode =
          entity->get_component<Engine::Core::HoldModeComponent>();
      if ((hold_mode != nullptr) && hold_mode->active) {
        hold_mode->active = false;
      }

      // Clear patrol
      if (auto *patrol =
              entity->get_component<Engine::Core::PatrolComponent>()) {
        patrol->patrolling = false;
        patrol->waypoints.clear();
      }
    } else {
      // Disable guard mode
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

  emit guard_modeChanged(should_enable_guard);

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

auto CommandController::onGuardClick(qreal sx, qreal sy, int viewportWidth,
                                     int viewportHeight,
                                     void *camera) -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_pickingService == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    result.resetCursorToNormal = true;
    return result;
  }

  const auto &selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    result.resetCursorToNormal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screen_to_ground(
          QPointF(sx, sy), *cam, viewportWidth, viewportHeight, hit)) {
    result.resetCursorToNormal = true;
    return result;
  }

  // Set guard position for all selected units
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

    // Disable hold mode if active
    auto *hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
    }

    // Clear patrol
    if (auto *patrol =
            entity->get_component<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    resetMovement(entity);
    entity->remove_component<Engine::Core::AttackTargetComponent>();
  }

  emit guard_modeChanged(true);

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

} // namespace App::Controllers
