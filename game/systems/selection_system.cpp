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
#include "units/spawn_type.h"
#include <QPointF>
#include <QVector3D>
#include <algorithm>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qvectornd.h>
#include <vector>

namespace Game::Systems {

void SelectionSystem::update(Engine::Core::World *world, float deltaTime) {}

void SelectionSystem::selectUnit(Engine::Core::EntityID unit_id) {
  auto it = std::find(m_selectedUnits.begin(), m_selectedUnits.end(), unit_id);
  if (it == m_selectedUnits.end()) {
    m_selectedUnits.push_back(unit_id);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitSelectedEvent(unit_id));
  }
}

void SelectionSystem::deselectUnit(Engine::Core::EntityID unit_id) {
  auto it = std::find(m_selectedUnits.begin(), m_selectedUnits.end(), unit_id);
  if (it != m_selectedUnits.end()) {
    m_selectedUnits.erase(it);
  }
}

void SelectionSystem::clearSelection() { m_selectedUnits.clear(); }

void SelectionSystem::selectUnitsInArea(float x1, float y1, float x2,
                                        float y2) {}

auto SelectionSystem::isUnitInArea(Engine::Core::Entity *entity, float x1,
                                   float y1, float x2, float y2) -> bool {
  auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return false;
  }

  float const x = transform->position.x;
  float const z = transform->position.z;

  return x >= std::min(x1, x2) && x <= std::max(x1, x2) &&
         z >= std::min(y1, y2) && z <= std::max(y1, y2);
}

SelectionController::SelectionController(Engine::Core::World *world,
                                         SelectionSystem *selection_system,
                                         PickingService *pickingService,
                                         QObject *parent)
    : QObject(parent), m_world(world), m_selection_system(selection_system),
      m_pickingService(pickingService) {}

void SelectionController::onClickSelect(qreal sx, qreal sy, bool additive,
                                        int viewportWidth, int viewportHeight,
                                        void *camera, int localOwnerId) {
  if ((m_selection_system == nullptr) || (m_pickingService == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID const picked = m_pickingService->pickSingle(
      float(sx), float(sy), *m_world, *cam, viewportWidth, viewportHeight,
      localOwnerId, true);

  if (picked != 0u) {
    if (!additive) {
      m_selection_system->clearSelection();
    }
    m_selection_system->selectUnit(picked);
    syncSelectionFlags();
    emit selectionChanged();
    return;
  }

  const auto &selected = m_selection_system->getSelectedUnits();
  if (!selected.empty()) {
    QVector3D hit;
    if ((m_pickingService == nullptr) ||
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
  if ((m_selection_system == nullptr) || (m_pickingService == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return;
  }

  if (!additive) {
    m_selection_system->clearSelection();
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  auto picked = m_pickingService->pickInRect(
      float(x1), float(y1), float(x2), float(y2), *m_world, *cam, viewportWidth,
      viewportHeight, localOwnerId);
  for (auto id : picked) {
    m_selection_system->selectUnit(id);
  }
  syncSelectionFlags();
  emit selectionChanged();
}

void SelectionController::onRightClickClearSelection() {
  if (m_selection_system == nullptr) {
    return;
  }
  m_selection_system->clearSelection();
  syncSelectionFlags();
  emit selectionChanged();
}

void SelectionController::selectAllPlayerTroops(int localOwnerId) {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return;
  }

  m_selection_system->clearSelection();

  auto entities = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != localOwnerId) {
      continue;
    }

    if (e->hasComponent<Engine::Core::BuildingComponent>()) {
      continue;
    }

    if (unit->health <= 0) {
      continue;
    }

    m_selection_system->selectUnit(e->getId());
  }

  syncSelectionFlags();
  emit selectionChanged();
}

auto SelectionController::hasUnitsSelected() const -> bool {
  if (m_selection_system == nullptr) {
    return false;
  }
  const auto &sel = m_selection_system->getSelectedUnits();
  return !sel.empty();
}

void SelectionController::getSelectedUnitIds(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (m_selection_system == nullptr) {
    return;
  }
  const auto &ids = m_selection_system->getSelectedUnits();
  out.assign(ids.begin(), ids.end());
}

auto SelectionController::hasSelectedType(const QString &type) const -> bool {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return false;
  }
  const auto &sel = m_selection_system->getSelectedUnits();
  for (auto id : sel) {
    if (auto *e = m_world->getEntity(id)) {
      if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
        if (QString::fromStdString(
                Game::Units::spawn_typeToString(u->spawn_type)) == type) {
          return true;
        }
      }
    }
  }
  return false;
}

void SelectionController::syncSelectionFlags() {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return;
  }
  App::Utils::sanitizeSelection(m_world, m_selection_system);
}

} // namespace Game::Systems