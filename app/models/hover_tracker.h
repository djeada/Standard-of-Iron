#pragma once

#include "game/systems/picking_service.h"
#include <memory>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
} 
} 

namespace Render {
namespace GL {
class Camera;
}
} 

class HoverTracker {
public:
  HoverTracker(Game::Systems::PickingService *pickingService);

  Engine::Core::EntityID updateHover(float sx, float sy,
                                     Engine::Core::World &world,
                                     const Render::GL::Camera &camera,
                                     int viewportWidth, int viewportHeight);

  Engine::Core::EntityID getLastHoveredEntity() const {
    return m_hoveredEntityId;
  }

private:
  Game::Systems::PickingService *m_pickingService;
  Engine::Core::EntityID m_hoveredEntityId = 0;
};
