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

  const auto &selected = m_selection_system->getSelectedUnits();
  if (selected.empty()) {
    result.resetCursorToNormal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID const target_id = m_pickingService->pickUnitFirst(
      float(sx), float(sy), *m_world, *cam, viewportWidth, viewportHeight, 0);

  if (target_id == 0) {
    result.resetCursorToNormal = true;
    return result;
  }

  auto *target_entity = m_world->getEntity(target_id);
  if (target_entity == nullptr) {
    return result;
  }

  auto *target_unit =
      target_entity->getComponent<Engine::Core::UnitComponent>();
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

  const auto &selected = m_selection_system->getSelectedUnits();
  if (selected.empty()) {
    return result;
  }

  for (auto id : selected) {
    auto *entity = m_world->getEntity(id);
    if (entity == nullptr) {
      continue;
    }

    resetMovement(entity);
    entity->removeComponent<Engine::Core::AttackTargetComponent>();

    if (auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    auto *hold_mode = entity->getComponent<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
      hold_mode->exitCooldown = hold_mode->standUpDuration;
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

  const auto &selected = m_selection_system->getSelectedUnits();
  if (selected.empty()) {
    return result;
  }

  for (auto id : selected) {
    auto *entity = m_world->getEntity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();

    if ((unit == nullptr) ||
        (unit->spawn_type != Game::Units::SpawnType::Archer &&
         unit->spawn_type != Game::Units::SpawnType::Spearman)) {
      continue;
    }

    auto *hold_mode = entity->getComponent<Engine::Core::HoldModeComponent>();

    if ((hold_mode != nullptr) && hold_mode->active) {
      hold_mode->active = false;
      hold_mode->exitCooldown = hold_mode->standUpDuration;
      emit hold_modeChanged(false);
      continue;
    }

    resetMovement(entity);
    entity->removeComponent<Engine::Core::AttackTargetComponent>();

    if (auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }

    if (hold_mode == nullptr) {
      hold_mode = entity->addComponent<Engine::Core::HoldModeComponent>();
    }
    hold_mode->active = true;
    hold_mode->exitCooldown = 0.0F;
    emit hold_modeChanged(true);

    auto *movement = entity->getComponent<Engine::Core::MovementComponent>();
    if (movement != nullptr) {
      movement->hasTarget = false;
      movement->path.clear();
      movement->pathPending = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
  }

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

  const auto &selected = m_selection_system->getSelectedUnits();
  if (selected.empty()) {
    if (m_hasPatrolFirstWaypoint) {
      clearPatrolFirstWaypoint();
      result.resetCursorToNormal = true;
    }
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screenToGround(
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
    auto *entity = m_world->getEntity(id);
    if (entity == nullptr) {
      continue;
    }

    auto *building = entity->getComponent<Engine::Core::BuildingComponent>();
    if (building != nullptr) {
      continue;
    }

    auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>();
    if (patrol == nullptr) {
      patrol = entity->addComponent<Engine::Core::PatrolComponent>();
    }

    if (patrol != nullptr) {
      patrol->waypoints.clear();
      patrol->waypoints.emplace_back(m_patrolFirstWaypoint.x(),
                                     m_patrolFirstWaypoint.z());
      patrol->waypoints.emplace_back(second_waypoint.x(), second_waypoint.z());
      patrol->currentWaypoint = 0;
      patrol->patrolling = true;
    }

    resetMovement(entity);
    entity->removeComponent<Engine::Core::AttackTargetComponent>();
  }

  clearPatrolFirstWaypoint();
  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

auto CommandController::setRallyAtScreen(qreal sx, qreal sy, int viewportWidth,
                                         int viewportHeight, void *camera,
                                         int localOwnerId) -> CommandResult {
  CommandResult result;
  if ((m_world == nullptr) || (m_selection_system == nullptr) ||
      (m_pickingService == nullptr) || (camera == nullptr)) {
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!Game::Systems::PickingService::screenToGround(
          QPointF(sx, sy), *cam, viewportWidth, viewportHeight, hit)) {
    return result;
  }

  Game::Systems::ProductionService::setRallyForFirstSelectedBarracks(
      *m_world, m_selection_system->getSelectedUnits(), localOwnerId, hit.x(),
      hit.z());

  result.inputConsumed = true;
  return result;
}

void CommandController::recruitNearSelected(const QString &unit_type,
                                            int localOwnerId) {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return;
  }

  const auto &sel = m_selection_system->getSelectedUnits();
  if (sel.empty()) {
    return;
  }

  auto result =
      Game::Systems::ProductionService::startProductionForFirstSelectedBarracks(
          *m_world, sel, localOwnerId, unit_type.toStdString());

  if (result == Game::Systems::ProductionResult::GlobalTroopLimitReached) {
    emit troopLimitReached();
  }
}

void CommandController::resetMovement(Engine::Core::Entity *entity) {
  App::Utils::resetMovement(entity);
}

auto CommandController::anySelectedInHoldMode() const -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto &selected = m_selection_system->getSelectedUnits();
  for (Engine::Core::EntityID const entity_id : selected) {
    Engine::Core::Entity *entity = m_world->getEntity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto *hold_mode = entity->getComponent<Engine::Core::HoldModeComponent>();
    if ((hold_mode != nullptr) && hold_mode->active) {
      return true;
    }
  }

  return false;
}

} // namespace App::Controllers
