#include "selection_system.h"

#include <algorithm>
#include <vector>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "units/spawn_type.h"

namespace Game::Systems {

void SelectionSystem::update(Engine::Core::World* world, float delta_time) {
}

void SelectionSystem::select_unit(Engine::Core::EntityID unit_id) {
  auto it = std::find(m_selected_units.begin(), m_selected_units.end(), unit_id);
  if (it == m_selected_units.end()) {
    m_inspected_entity = Engine::Core::NULL_ENTITY;
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

void SelectionSystem::clear_selection() {
  m_selected_units.clear();
  m_inspected_entity = Engine::Core::NULL_ENTITY;
}

void SelectionSystem::select_units_in_area(float x1, float y1, float x2, float y2) {
}

auto SelectionSystem::is_unit_in_area(
    Engine::Core::Entity* entity, float x1, float y1, float x2, float y2) -> bool {
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return false;
  }

  float const x = transform->position.x;
  float const z = transform->position.z;

  return x >= std::min(x1, x2) && x <= std::max(x1, x2) && z >= std::min(y1, y2) &&
         z <= std::max(y1, y2);
}

} // namespace Game::Systems
