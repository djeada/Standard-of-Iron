#include "command_controller.h"
#include "utils/movement_utils.h"
#include "game/core/world.h"
#include "game/core/entity.h"
#include "game/core/component.h"
#include "game/systems/selection_system.h"
#include "game/systems/picking_service.h"
#include "game/systems/command_service.h"
#include "game/systems/production_service.h"
#include "game/visuals/action_vfx.h"
#include "render/gl/camera.h"
#include <QQuickWindow>

CommandController::CommandController(Engine::Core::World *world,
                                    Game::Systems::PickingService *pickingService,
                                    QObject *parent)
    : QObject(parent), m_world(world), m_pickingService(pickingService) {}

void CommandController::resetMovement(Engine::Core::Entity *entity) {
  App::Utils::resetMovement(entity);
}

CommandResult CommandController::onAttackClick(qreal sx, qreal sy,
                                              QQuickWindow *window,
                                              int viewportWidth, int viewportHeight,
                                              int localOwnerId,
                                              Render::GL::Camera *camera) {
  CommandResult result;
  if (!window || !m_pickingService || !camera || !m_world) {
    result.resetCursorToNormal = true;
    return result;
  }

  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem) {
    result.resetCursorToNormal = true;
    return result;
  }

  const auto &selected = selectionSystem->getSelectedUnits();
  if (selected.empty()) {
    result.resetCursorToNormal = true;
    return result;
  }

  Engine::Core::EntityID targetId =
      m_pickingService->pickUnitFirst(float(sx), float(sy), *m_world, *camera,
                                      viewportWidth, viewportHeight, 0);

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

  if (targetUnit->ownerId == localOwnerId) {
    return result;
  }

  Game::Systems::CommandService::attackTarget(*m_world, selected, targetId, true);

  Game::Visuals::ActionVFX::spawnAttackArrow(*m_world, targetEntity);

  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

CommandResult CommandController::onStopCommand() {
  CommandResult result;
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem || !m_world)
    return result;

  const auto &selected = selectionSystem->getSelectedUnits();
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
                                              QQuickWindow *window,
                                              std::function<bool(const QPointF&, QVector3D&)> screenToGround) {
  CommandResult result;
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem || !m_world)
    return result;

  const auto &selected = selectionSystem->getSelectedUnits();
  if (selected.empty()) {
    if (m_hasPatrolFirstWaypoint) {
      m_hasPatrolFirstWaypoint = false;
      result.resetCursorToNormal = true;
    }
    return result;
  }

  QVector3D hit;
  if (!screenToGround(QPointF(sx, sy), hit)) {
    if (m_hasPatrolFirstWaypoint) {
      m_hasPatrolFirstWaypoint = false;
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
      patrol->waypoints.push_back({m_patrolFirstWaypoint.x(), m_patrolFirstWaypoint.z()});
      patrol->waypoints.push_back({secondWaypoint.x(), secondWaypoint.z()});
      patrol->currentWaypoint = 0;
      patrol->patrolling = true;
    }

    resetMovement(entity);
    entity->removeComponent<Engine::Core::AttackTargetComponent>();
  }

  m_hasPatrolFirstWaypoint = false;
  result.inputConsumed = true;
  result.resetCursorToNormal = true;
  return result;
}

CommandResult CommandController::setRallyAtScreen(qreal sx, qreal sy,
                                                  std::function<bool(const QPointF&, QVector3D&)> screenToGround,
                                                  int localOwnerId) {
  CommandResult result;
  if (!m_world)
    return result;
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem)
    return result;

  QVector3D hit;
  if (!screenToGround(QPointF(sx, sy), hit))
    return result;

  Game::Systems::ProductionService::setRallyForFirstSelectedBarracks(
      *m_world, selectionSystem->getSelectedUnits(), localOwnerId,
      hit.x(), hit.z());

  result.inputConsumed = true;
  return result;
}

CommandResult CommandController::recruitNearSelected(const QString &unitType,
                                                    int localOwnerId) {
  CommandResult result;
  if (!m_world)
    return result;
  auto *selectionSystem = m_world->getSystem<Game::Systems::SelectionSystem>();
  if (!selectionSystem)
    return result;

  const auto &sel = selectionSystem->getSelectedUnits();
  if (sel.empty())
    return result;

  Game::Systems::ProductionService::startProductionForFirstSelectedBarracks(
      *m_world, sel, localOwnerId, unitType.toStdString());

  result.inputConsumed = true;
  return result;
}
