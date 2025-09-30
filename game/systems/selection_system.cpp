#include "selection_system.h"
#include "../core/world.h"
#include "../core/component.h"
#include <algorithm>

namespace Game::Systems {

void SelectionSystem::update(Engine::Core::World* world, float deltaTime) {
    // Update selection visualization or handle selection-related logic
    // This system is primarily event-driven through direct method calls
}

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

void SelectionSystem::clearSelection() {
    m_selectedUnits.clear();
}

void SelectionSystem::selectUnitsInArea(float x1, float y1, float x2, float y2) {
    // This would typically be called by the input system
    // For now, it's a stub that would need world access to work properly
    // In a real implementation, you'd pass the world or store a reference
}

bool SelectionSystem::isUnitInArea(Engine::Core::Entity* entity, float x1, float y1, float x2, float y2) {
    auto transform = entity->getComponent<Engine::Core::TransformComponent>();
    if (!transform) {
        return false;
    }
    
    float x = transform->position.x;
    float z = transform->position.z;
    
    return x >= std::min(x1, x2) && x <= std::max(x1, x2) &&
           z >= std::min(y1, y2) && z <= std::max(y1, y2);
}

} // namespace Game::Systems