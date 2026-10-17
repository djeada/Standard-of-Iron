#include "selection_service.h"

#include <algorithm>
#include <utility>

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

void SelectionService::set_control_group(int group,
                                         std::vector<Engine::Core::EntityID> members) {
  if (!is_control_group(group)) {
    return;
  }
  m_control_groups[static_cast<std::size_t>(group)] = std::move(members);
}

auto SelectionService::control_group(int group) const
    -> const std::vector<Engine::Core::EntityID>& {
  static const std::vector<Engine::Core::EntityID> k_empty;
  if (!is_control_group(group)) {
    return k_empty;
  }
  return m_control_groups[static_cast<std::size_t>(group)];
}

void SelectionService::clear_control_groups() {
  for (auto& members : m_control_groups) {
    members.clear();
  }
}

} // namespace Game::Session
