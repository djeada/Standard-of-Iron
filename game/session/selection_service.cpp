#include "selection_service.h"

#include <algorithm>

#include "../core/event_manager.h"

namespace Game::Session {

void SelectionService::select_unit(Engine::Core::EntityID unit_id) {
  auto it = std::find(m_selected_units.begin(), m_selected_units.end(), unit_id);
  if (it == m_selected_units.end()) {
    m_inspected_entity = Engine::Core::NULL_ENTITY;
    m_selected_units.push_back(unit_id);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitSelectedEvent(unit_id));
  }
}

void SelectionService::deselect_unit(Engine::Core::EntityID unit_id) {
  auto it = std::find(m_selected_units.begin(), m_selected_units.end(), unit_id);
  if (it != m_selected_units.end()) {
    m_selected_units.erase(it);
  }
}

void SelectionService::clear_selection() {
  m_selected_units.clear();
  m_inspected_entity = Engine::Core::NULL_ENTITY;
}

} // namespace Game::Session
