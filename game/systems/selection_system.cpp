#include "selection_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "picking_service.h"
#include "command_service.h"
#include "formation_planner.h"
#include "../game_config.h"
#include "../../render/gl/camera.h"
#include <algorithm>
#include <QQuickWindow>
#include <QVector3D>
#include <QPointF>

namespace Game::Systems {

void SelectionSystem::update(Engine::Core::World *world, float deltaTime) {}

void SelectionSystem::selectUnit(Engine::Core::EntityID unitId) {
  auto it = std::find(m_selectedUnits.begin(), m_selectedUnits.end(), unitId);
  if (it == m_selectedUnits.end()) {
    m_selectedUnits.push_back(unitId);
  }
}

void SelectionSystem::deselectUnit(Engine::Core::EntityID unitId) {
  auto it = std::find(m_selectedUnits.begin(), m_selectedUnits.end(), unitId);
  if (it != m_selectedUnits.end()) {
    m_selectedUnits.erase(it);
  }
}

void SelectionSystem::clearSelection() { m_selectedUnits.clear(); }

void SelectionSystem::selectUnitsInArea(float x1, float y1, float x2,
                                        float y2) {}

bool SelectionSystem::isUnitInArea(Engine::Core::Entity *entity, float x1,
                                   float y1, float x2, float y2) {
  auto transform = entity->getComponent<Engine::Core::TransformComponent>();
  if (!transform) {
    return false;
  }

  float x = transform->position.x;
  float z = transform->position.z;

  return x >= std::min(x1, x2) && x <= std::max(x1, x2) &&
         z >= std::min(y1, y2) && z <= std::max(y1, y2);
}

SelectionController::SelectionController(Engine::Core::World *world,
                                       PickingService *pickingService,
                                       QObject *parent)
    : QObject(parent), m_world(world), m_pickingService(pickingService) {}

void SelectionController::onClickSelect(qreal sx, qreal sy, bool additive,
                                       QQuickWindow *window,
                                       int viewportWidth, int viewportHeight,
                                       int localOwnerId, Render::GL::Camera *camera,
                                       std::function<bool(const QPointF&, QVector3D&)> screenToGround) {
  auto *selectionSystem = m_world->getSystem<SelectionSystem>();
  if (!window || !selectionSystem || !m_pickingService || !camera)
    return;

  Engine::Core::EntityID picked = m_pickingService->pickSingle(
      float(sx), float(sy), *m_world, *camera,
      viewportWidth, viewportHeight, localOwnerId, true);

  if (picked) {
    if (!additive)
      selectionSystem->clearSelection();
    selectionSystem->selectUnit(picked);
    emit selectionChanged();
    emit selectionModelRefreshRequested();
    m_selectionRefreshCounter = 0;
    return;
  }

  const auto &selected = selectionSystem->getSelectedUnits();
  if (!selected.empty()) {
    QVector3D hit;
    if (!screenToGround(QPointF(sx, sy), hit)) {
      return;
    }
    auto targets = FormationPlanner::spreadFormation(
        int(selected.size()), hit,
        Game::GameConfig::instance().gameplay().formationSpacingDefault);
    CommandService::MoveOptions opts;
    opts.groupMove = selected.size() > 1;
    CommandService::moveUnits(*m_world, selected, targets, opts);
    return;
  }
}

void SelectionController::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2,
                                        bool additive, QQuickWindow *window,
                                        int viewportWidth, int viewportHeight,
                                        int localOwnerId, Render::GL::Camera *camera) {
  auto *selectionSystem = m_world->getSystem<SelectionSystem>();
  if (!window || !selectionSystem || !m_pickingService || !camera)
    return;

  if (!additive)
    selectionSystem->clearSelection();

  auto picked = m_pickingService->pickInRect(
      float(x1), float(y1), float(x2), float(y2), *m_world,
      *camera, viewportWidth, viewportHeight,
      localOwnerId);

  for (auto id : picked)
    selectionSystem->selectUnit(id);

  emit selectionChanged();
  emit selectionModelRefreshRequested();
  m_selectionRefreshCounter = 0;
}

bool SelectionController::onRightClickClearSelection() {
  auto *selectionSystem = m_world->getSystem<SelectionSystem>();
  if (!selectionSystem)
    return false;

  const auto &sel = selectionSystem->getSelectedUnits();
  if (!sel.empty()) {
    selectionSystem->clearSelection();
    emit selectionChanged();
    emit selectionModelRefreshRequested();
    m_selectionRefreshCounter = 0;
    return true;
  }
  return false;
}

bool SelectionController::hasUnitsSelected() const {
  if (!m_world)
    return false;
  auto *selectionSystem = m_world->getSystem<SelectionSystem>();
  if (!selectionSystem)
    return false;
  const auto &sel = selectionSystem->getSelectedUnits();
  return !sel.empty();
}

void SelectionController::getSelectedUnitIds(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (!m_world)
    return;
  auto *selectionSystem = m_world->getSystem<SelectionSystem>();
  if (!selectionSystem)
    return;
  const auto &ids = selectionSystem->getSelectedUnits();
  out.assign(ids.begin(), ids.end());
}

bool SelectionController::hasSelectedType(const QString &type) const {
  if (!m_world)
    return false;
  auto *selectionSystem = m_world->getSystem<SelectionSystem>();
  if (!selectionSystem)
    return false;
  const auto &sel = selectionSystem->getSelectedUnits();
  for (auto id : sel) {
    if (auto *e = m_world->getEntity(id)) {
      if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
        if (QString::fromStdString(u->unitType) == type)
          return true;
      }
    }
  }
  return false;
}

} // namespace Game::Systems