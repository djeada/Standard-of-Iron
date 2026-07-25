#pragma once

#include <memory>

#include "game/systems/picking_service.h"

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

class HoverTracker {
public:
  HoverTracker(Game::Systems::PickingService* picking_service);

  auto update_hover(float sx,
                    float sy,
                    Engine::Core::World& world,
                    const Render::GL::Camera& camera,
                    int viewport_width,
                    int viewport_height) -> Engine::Core::EntityID;

  [[nodiscard]] auto get_last_hovered_entity() const -> Engine::Core::EntityID {
    return m_hovered_entity_id;
  }

private:
  Game::Systems::PickingService* m_picking_service;
  Engine::Core::EntityID m_hovered_entity_id = 0;
};
