#include "selection_system.h"
#include "../../app/utils/selection_utils.h"
#include "../../render/gl/camera.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "command_service.h"
#include "picking_service.h"
#include "units/spawn_type.h"
#include <QPointF>
#include <QVector3D>
#include <algorithm>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <vector>

namespace Game::Systems {

void SelectionSystem::update(Engine::Core::World *world, float delta_time) {}

void SelectionSystem::select_unit(Engine::Core::EntityID unit_id) {
  auto it = std::find(m_selected_units.begin(), m_selected_units.end(), unit_id);
  if (it == m_selected_units.end()) {
    m_selected_units.push_back(unit_id);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitSelectedEvent(unit_id));
  }
}

void SelectionSystem::deselect_unit(Engine::Core::EntityID unit_id) {
  auto it = std::find(m_selected_units.begin(), m_selected_units.end(), unit_id);
  if (it != m_selected_units.end()) {
    m_selected_units.erase(it);
  }
}

void SelectionSystem::clear_selection() { m_selected_units.clear(); }

void SelectionSystem::select_unitsInArea(float x1, float y1, float x2,
                                        float y2) {}

auto SelectionSystem::is_unit_in_area(Engine::Core::Entity *entity, float x1,
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
      m_picking_service(pickingService) {}

void SelectionController::on_click_select(qreal sx, qreal sy, bool additive,
                                        int viewport_width, int viewport_height,
                                        void *camera, int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return;
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  Engine::Core::EntityID const picked =
      Game::Systems::PickingService::pickSingle(
          float(sx), float(sy), *m_world, *cam, viewport_width, viewport_height,
          local_owner_id, true);

  if (picked != 0U) {

    if (!additive) {
      m_selection_system->clearSelection();
    }
    m_selection_system->selectUnit(picked);
    syncSelectionFlags();
    emit selection_changed();
    return;
  }

  if (!additive && !m_selection_system->getSelectedUnits().empty()) {
    m_selection_system->clearSelection();
    syncSelectionFlags();
    emit selection_changed();
  }
}

void SelectionController::on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2,
                                         bool additive, int viewport_width,
                                         int viewport_height, void *camera,
                                         int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return;
  }

  if (!additive) {
    m_selection_system->clearSelection();
  }

  auto *cam = static_cast<Render::GL::Camera *>(camera);
  auto picked = Game::Systems::PickingService::pickInRect(
      float(x1), float(y1), float(x2), float(y2), *m_world, *cam, viewport_width,
      viewport_height, local_owner_id);
  for (auto id : picked) {
    m_selection_system->selectUnit(id);
  }
  syncSelectionFlags();
  emit selection_changed();
}

void SelectionController::on_right_click_clear_selection() {
  if (m_selection_system == nullptr) {
    return;
  }
  m_selection_system->clearSelection();
  syncSelectionFlags();
  emit selection_changed();
}

void SelectionController::select_all_player_troops(int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return;
  }

  m_selection_system->clearSelection();

  auto entities = m_world->getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id) {
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
  emit selection_changed();
}

void SelectionController::select_single_unit(Engine::Core::EntityID id,
                                           int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return;
  }

  auto *entity = m_world->getEntity(id);
  if (entity == nullptr) {
    return;
  }

  auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
  if ((unit == nullptr) || (unit->health <= 0) ||
      (unit->owner_id != local_owner_id)) {
    return;
  }

  m_selection_system->clearSelection();
  m_selection_system->selectUnit(id);
  syncSelectionFlags();
  emit selection_changed();
}

auto SelectionController::has_units_selected() const -> bool {
  if (m_selection_system == nullptr) {
    return false;
  }
  const auto &sel = m_selection_system->getSelectedUnits();
  return !sel.empty();
}

void SelectionController::get_selected_unit_ids(
    std::vector<Engine::Core::EntityID> &out) const {
  out.clear();
  if (m_selection_system == nullptr) {
    return;
  }
  const auto &ids = m_selection_system->getSelectedUnits();
  out.assign(ids.begin(), ids.end());
}

auto SelectionController::has_selected_type(const QString &type) const -> bool {
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

void SelectionController::sync_selection_flags() {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return;
  }
  App::Utils::sanitizeSelection(m_world, m_selection_system);
}

} // namespace Game::Systems
