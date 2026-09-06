#include "selection_controller.h"

#include <QPointF>
#include <QVector3D>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "../core/component_structures.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../render_bridge/picking_service.h"
#include "../systems/command_service.h"
#include "../units/spawn_type.h"
#include "../util/selection_utils.h"
#include "scene/camera.h"

namespace Game::Systems {

namespace {

void play_selection_cue(std::size_t selected_count) {
  if (selected_count == 0) {
    return;
  }
  Engine::Core::EventManager::instance().publish(Engine::Core::AudioCueEvent(
      selected_count > 1 ? "ui.select_group" : "ui.select_unit"));
}

void play_deselect_cue() {
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent("ui.deselect"));
}

} // namespace

SelectionController::SelectionController(Engine::Core::World* world,
                                         SelectionSystem* selection_system,
                                         PickingService* picking_service,
                                         QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_selection_system(selection_system)
    , m_picking_service(picking_service) {
}

void SelectionController::on_click_select(qreal sx,
                                          qreal sy,
                                          bool additive,
                                          int viewport_width,
                                          int viewport_height,
                                          void* camera,
                                          int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return;
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  Engine::Core::EntityID const picked =
      Game::Systems::PickingService::pick_single(float(sx),
                                                 float(sy),
                                                 *m_world,
                                                 *cam,
                                                 viewport_width,
                                                 viewport_height,
                                                 local_owner_id,
                                                 true);

  if (picked != 0U) {

    if (!additive) {
      m_selection_system->clear_selection();
    }
    m_selection_system->select_unit(picked);
    sync_selection_flags();
    play_selection_cue(m_selection_system->get_selected_units().size());
    emit selection_changed();
    return;
  }

  if (!additive) {
    Engine::Core::EntityID const foreign = Game::Systems::PickingService::pick_single(
        float(sx), float(sy), *m_world, *cam, viewport_width, viewport_height, 0, true);
    if (foreign != 0U && can_inspect(foreign, local_owner_id)) {
      const bool changed = m_selection_system->inspected_entity() != foreign ||
                           !m_selection_system->get_selected_units().empty();
      m_selection_system->clear_selection();
      m_selection_system->set_inspected_entity(foreign);
      sync_selection_flags();
      if (changed) {
        play_selection_cue(1);
        emit selection_changed();
      }
      return;
    }
  }

  const bool had_focus = !m_selection_system->get_selected_units().empty() ||
                         m_selection_system->inspected_entity() != 0U;
  if (!additive && had_focus) {
    m_selection_system->clear_selection();
    sync_selection_flags();
    play_deselect_cue();
    emit selection_changed();
  }
}

auto SelectionController::can_inspect(Engine::Core::EntityID entity_id,
                                      int local_owner_id) const -> bool {
  if (m_world == nullptr) {
    return false;
  }
  auto* entity = m_world->get_entity(entity_id);
  if (entity == nullptr) {
    return false;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0) {
    return false;
  }
  if (unit->owner_id == local_owner_id) {
    return false;
  }
  return !m_inspect_filter || m_inspect_filter(entity_id);
}

void SelectionController::on_area_selected(qreal x1,
                                           qreal y1,
                                           qreal x2,
                                           qreal y2,
                                           bool additive,
                                           int viewport_width,
                                           int viewport_height,
                                           void* camera,
                                           int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_picking_service == nullptr) ||
      (camera == nullptr) || (m_world == nullptr)) {
    return;
  }

  const bool had_selection = !m_selection_system->get_selected_units().empty();
  if (!additive) {
    m_selection_system->clear_selection();
  }

  auto* cam = static_cast<Render::GL::Camera*>(camera);
  auto picked = Game::Systems::PickingService::pick_in_rect(float(x1),
                                                            float(y1),
                                                            float(x2),
                                                            float(y2),
                                                            *m_world,
                                                            *cam,
                                                            viewport_width,
                                                            viewport_height,
                                                            local_owner_id);
  for (auto id : picked) {
    m_selection_system->select_unit(id);
  }
  sync_selection_flags();
  if (picked.empty()) {
    if (had_selection && !additive) {
      play_deselect_cue();
    }
  } else {
    play_selection_cue(m_selection_system->get_selected_units().size());
  }
  emit selection_changed();
}

void SelectionController::on_right_click_clear_selection() {
  if (m_selection_system == nullptr) {
    return;
  }
  const bool had_selection = !m_selection_system->get_selected_units().empty();
  m_selection_system->clear_selection();
  sync_selection_flags();
  if (had_selection) {
    play_deselect_cue();
  }
  emit selection_changed();
}

void SelectionController::select_all_player_troops(int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return;
  }

  m_selection_system->clear_selection();

  auto entities = m_world->collect_entities_with<Engine::Core::UnitComponent>();
  for (auto* e : entities) {
    auto* unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != local_owner_id) {
      continue;
    }

    if (e->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    if (unit->health <= 0) {
      continue;
    }

    m_selection_system->select_unit(e->get_id());
  }

  sync_selection_flags();
  play_selection_cue(m_selection_system->get_selected_units().size());
  emit selection_changed();
}

void SelectionController::select_single_unit(Engine::Core::EntityID id,
                                             int local_owner_id) {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return;
  }

  auto* entity = m_world->get_entity(id);
  if (entity == nullptr) {
    return;
  }

  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if ((unit == nullptr) || (unit->health <= 0) || (unit->owner_id != local_owner_id)) {
    return;
  }

  m_selection_system->clear_selection();
  m_selection_system->select_unit(id);
  sync_selection_flags();
  play_selection_cue(m_selection_system->get_selected_units().size());
  emit selection_changed();
}

void SelectionController::select_selected_units_by_type(const QString& unit_type,
                                                        int local_owner_id) {
  if (m_selection_system == nullptr || m_world == nullptr || unit_type.isEmpty()) {
    return;
  }

  std::vector<Engine::Core::EntityID> matching;
  for (const auto id : m_selection_system->get_selected_units()) {
    auto* entity = m_world->get_entity(id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit == nullptr || unit->health <= 0 || unit->owner_id != local_owner_id ||
        entity->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    if (QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type)) ==
        unit_type) {
      matching.push_back(id);
    }
  }

  if (matching.empty()) {
    return;
  }

  m_selection_system->clear_selection();
  for (const auto id : matching) {
    m_selection_system->select_unit(id);
  }
  sync_selection_flags();
  play_selection_cue(m_selection_system->get_selected_units().size());
  emit selection_changed();
}

auto SelectionController::has_units_selected() const -> bool {
  if (m_selection_system == nullptr) {
    return false;
  }
  const auto& sel = m_selection_system->get_selected_units();
  return !sel.empty();
}

void SelectionController::get_selected_unit_ids(
    std::vector<Engine::Core::EntityID>& out) const {
  out.clear();
  if (m_selection_system == nullptr) {
    return;
  }
  const auto& ids = m_selection_system->get_selected_units();
  out.assign(ids.begin(), ids.end());
}

auto SelectionController::has_selected_type(const QString& type) const -> bool {
  if ((m_world == nullptr) || (m_selection_system == nullptr)) {
    return false;
  }
  const auto& sel = m_selection_system->get_selected_units();
  for (auto id : sel) {
    if (auto* e = m_world->get_entity(id)) {
      if (auto* u = e->get_component<Engine::Core::UnitComponent>()) {
        if (QString::fromStdString(Game::Units::spawn_typeToString(u->spawn_type)) ==
            type) {
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
  Game::Selection::sanitize_selection(m_world, m_selection_system);
}

} // namespace Game::Systems
