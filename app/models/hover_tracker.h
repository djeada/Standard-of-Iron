#pragma once

#include "game/systems/picking_service.h"
#include <memory>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

class HoverTracker {
public:
  HoverTracker(Game::Systems::PickingService *pickingService);

  auto updateHover(float sx, float sy, Engine::Core::World &world,
                   const Render::GL::Camera &camera, int viewportWidth,
                   int viewportHeight) -> Engine::Core::EntityID;

  [[nodiscard]] auto getLastHoveredEntity() const -> Engine::Core::EntityID {
    return m_hoveredEntityId;
  }

private:
  Game::Systems::PickingService *m_pickingService;
  Engine::Core::EntityID m_hoveredEntityId = 0;
};
