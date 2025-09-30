#pragma once

#include "../core/system.h"
#include "../core/entity.h"
#include <vector>

namespace Engine { namespace Core { class Entity; } }

namespace Game::Systems {

class SelectionSystem : public Engine::Core::System {
public:
    void update(Engine::Core::World* world, float deltaTime) override;
    
    // Selection management
    void selectUnit(Engine::Core::EntityID unitId);
    void deselectUnit(Engine::Core::EntityID unitId);
    void clearSelection();
    void selectUnitsInArea(float x1, float y1, float x2, float y2);
    
    const std::vector<Engine::Core::EntityID>& getSelectedUnits() const { return m_selectedUnits; }

private:
    std::vector<Engine::Core::EntityID> m_selectedUnits;
    bool isUnitInArea(Engine::Core::Entity* entity, float x1, float y1, float x2, float y2);
};

} // namespace Game::Systems