#include "selection_system.h"
#include "../../app/utils/selection_utils.h"
#include "../../render/gl/camera.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../game_config.h"
#include "command_service.h"
#include "formation_planner.h"
#include "picking_service.h"
#include <QPointF>
#include <QVector3D>
#include <algorithm>

namespace Game::Systems {

void SelectionSystem::update(Engine::Core::World *world, float deltaTime) {}

void SelectionSystem::selectUnit(Engine::Core::EntityID unitId) {
  auto it = std::find(m_selectedUnits.begin(), m_selectedUnits.end(), unitId);
  if (it == m_selectedUnits.end()) {
    m_selectedUnits.push_back(unitId);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitSelectedEvent(unitId));
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
                                         SelectionSystem *selectionSystem,
                                         PickingService *pickingService,
                                         QObject *parent)
    : QObject(parent), m_world(world), m_selectionSystem(selectionSystem),
      m_pickingService(pickingService) {}

void SelectionController::onClickSelect(qreal sx, qreal sy, bool additive,
                                        int viewportWidth, int viewportHeight,
                                        void *camera, int localOwnerId) {
  if (!m_selectionSystem || !m_pickingService || !camera || !m_world)
    return;

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID picked = m_pickingService->pickSingle(
      float(sx), float(sy), *m_world, *cam, viewportWidth, viewportHeight,
      localOwnerId, true);

  if (picked) {
    if (!additive)
      m_selectionSystem->clearSelection();
    m_selectionSystem->selectUnit(picked);
    syncSelectionFlags();
    emit selectionChanged();
    return;
  }

  const auto &selected = m_selectionSystem->getSelectedUnits();
  if (!selected.empty()) {
    QVector3D hit;
    if (!m_pickingService ||
        !m_pickingService->screenToGround(QPointF(sx, sy), *cam, viewportWidth,
                                          viewportHeight, hit)) {
      return;
    }
    auto targets = Game::Systems::FormationPlanner::spreadFormation(
        int(selected.size()), hit,
        Game::GameConfig::instance().gameplay().formationSpacingDefault);
    Game::Systems::CommandService::MoveOptions opts;
    opts.groupMove = selected.size() > 1;
    Game::Systems::CommandService::moveUnits(*m_world, selected, targets, opts);
    syncSelectionFlags();
    return;
  }
}

void SelectionController::onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2,
                                         bool additive, int viewportWidth,
                                         int viewportHeight, void *camera,
                                         int localOwnerId) {
  if (!m_selectionSystem || !m_pickingService || !camera || !m_world)
    return;

  if (!additive)
    m_selectionSystem->clearSelection();

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  auto picked = m_pickingService->pickInRect(
      float(x1), float(y1), float(x2), float(y2), *m_world, *cam, viewportWidth,
      viewportHeight, localOwnerId);
  for (auto id : picked)
    m_selectionSystem->selectUnit(id);
  syncSelectionFlags();
  emit selectionChanged();
}

void SelectionController::onRightClickClearSelection() {
  if (!m_selectionSystem)
    return;
  m_selectionSystem->clearSelection();
  syncSelectionFlags();
  emit selectionChanged();
}

void SelectionController::selectAllPlayerTroops(int localOwnerId) {
  if (!m_selectionSystem || !m_world)
    return;

  m_selectionSystem->clearSelection();

  auto entities = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->ownerId != localOwnerId)
      continue;

    if (e->hasComponent<Engine::Core::BuildingComponent>())
      continue;

    if (unit->health <= 0)
      continue;

    m_selectionSystem->selectUnit(e->getId());
  }

  syncSelectionFlags();
  emit selectionChanged();
}

bool SelectionController::hasUnitsSelected() const {
  if (!m_selectionSystem)
    return false;
  const auto &sel = m_selectionSystem->getSelectedUnits();
  return !sel.empty();
}

void SelectionController::getSelectedUnitIds(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (!m_selectionSystem)
    return;
  const auto &ids = m_selectionSystem->getSelectedUnits();
  out.assign(ids.begin(), ids.end());
}

bool SelectionController::hasSelectedType(const QString &type) const {
  if (!m_world || !m_selectionSystem)
    return false;
  const auto &sel = m_selectionSystem->getSelectedUnits();
  for (auto id : sel) {
    if (auto *e = m_world->getEntity(id)) {
      if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
        if (QString::fromStdString(
                Game::Units::spawnTypeToString(u->spawnType)) == type)
          return true;
      }
    }
  }
  return false;
}

void SelectionController::syncSelectionFlags() {
  if (!m_world || !m_selectionSystem)
    return;
  App::Utils::sanitizeSelection(m_world, m_selectionSystem);
}

} // namespace Game::Systems