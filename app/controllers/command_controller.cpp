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
#include <QPointF>

namespace App::Controllers {

CommandController::CommandController(
    Engine::Core::World *world,
    Game::Systems::SelectionSystem *selectionSystem,
    Game::Systems::PickingService *pickingService, QObject *parent)
    : QObject(parent), m_world(world), m_selectionSystem(selectionSystem),
      m_pickingService(pickingService) {}

CommandResult CommandController::onAttackClick(qreal sx, qreal sy,
                                              int viewportWidth,
                                              int viewportHeight, void *camera) {
  CommandResult result;
  if (!m_selectionSystem || !m_pickingService || !camera || !m_world) {
    result.resetCursorToNormal = true;
    return result;
  }

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (selected.empty()) {
    result.resetCursorToNormal = true;
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID targetId = m_pickingService->pickUnitFirst(
      float(sx), float(sy), *m_world, *cam, viewportWidth, viewportHeight, 0);

  if (targetId == 0) {
    result.resetCursorToNormal = true;
    return result;
  }

  auto *targetEntity = m_world->getEntity(targetId);
  if (!targetEntity) {
    return result;
  }

  auto *targetUnit = targetEntity->getComponent<Engine::Core::UnitComponent>();
  if (!targetUnit) {
    return result;
  }

  Game::Systems::CommandService::attackTarget(*m_world, selected, targetId,
                                              true);

  emit attackTargetSelected();

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

CommandResult CommandController::onStopCommand() {
  CommandResult result;
  if (!m_selectionSystem || !m_world) {
    return result;
  }

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (selected.empty())
    return result;

  for (auto id : selected) {
    auto *entity = m_world->getEntity(id);
    if (!entity)
      continue;

    resetMovement(entity);

    entity->removeComponent<Engine::Core::AttackTargetComponent>();

    if (auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>()) {
      patrol->patrolling = false;
      patrol->waypoints.clear();
    }
  }

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

CommandResult CommandController::onPatrolClick(qreal sx, qreal sy,
                                              int viewportWidth,
                                              int viewportHeight, void *camera) {
  CommandResult result;
  if (!m_selectionSystem || !m_world || !m_pickingService || !camera) {
    if (m_hasPatrolFirstWaypoint) {
      clearPatrolFirstWaypoint();
      result.resetCursorToNormal = true;
    }
    return result;
  }

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (selected.empty()) {
    if (m_hasPatrolFirstWaypoint) {
      clearPatrolFirstWaypoint();
      result.resetCursorToNormal = true;
    }
    return result;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!m_pickingService->screenToGround(QPointF(sx, sy), *cam, viewportWidth,
                                       viewportHeight, hit)) {
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

  QVector3D secondWaypoint = hit;

  for (auto id : selected) {
    auto *entity = m_world->getEntity(id);
    if (!entity)
      continue;

    auto *building = entity->getComponent<Engine::Core::BuildingComponent>();
    if (building)
      continue;

    auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>();
    if (!patrol) {
      patrol = entity->addComponent<Engine::Core::PatrolComponent>();
    }

    if (patrol) {
      patrol->waypoints.clear();
      patrol->waypoints.push_back(
          {m_patrolFirstWaypoint.x(), m_patrolFirstWaypoint.z()});
      patrol->waypoints.push_back({secondWaypoint.x(), secondWaypoint.z()});
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

CommandResult CommandController::setRallyAtScreen(qreal sx, qreal sy,
                                                 int viewportWidth,
                                                 int viewportHeight,
                                                 void *camera,
                                                 int localOwnerId) {
  CommandResult result;
  if (!m_world || !m_selectionSystem || !m_pickingService || !camera)
    return result;

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  QVector3D hit;
  if (!m_pickingService->screenToGround(QPointF(sx, sy), *cam, viewportWidth,
                                       viewportHeight, hit))
    return result;

  Game::Systems::ProductionService::setRallyForFirstSelectedBarracks(
      *m_world, m_selectionSystem->getSelectedUnits(), localOwnerId, hit.x(),
      hit.z());

  result.inputConsumed = true;
  return result;
}

void CommandController::recruitNearSelected(const QString &unitType,
                                           int localOwnerId) {
  if (!m_world || !m_selectionSystem)
    return;

  const auto &sel = m_selectionSystem->getSelectedUnits();
  if (sel.empty())
    return;

  auto result = Game::Systems::ProductionService::startProductionForFirstSelectedBarracks(
      *m_world, sel, localOwnerId, unitType.toStdString());
  
  if (result == Game::Systems::ProductionResult::GlobalTroopLimitReached) {
    emit troopLimitReached();
  }
}

void CommandController::resetMovement(Engine::Core::Entity *entity) {
  App::Utils::resetMovement(entity);
}

} // namespace App::Controllers
