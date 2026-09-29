#pragma once

#include <QString>

#include <cstdint>

#include "app/input/viewport_state.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {
class PickingService;
}

class HoverTracker;

struct ContextInteraction {

  QString gather_product_type;

  QString food_product_type;

  Engine::Core::EntityID target = 0;

  [[nodiscard]] auto is_gather() const -> bool {
    return !gather_product_type.isEmpty();
  }
  [[nodiscard]] auto is_food_task() const -> bool {
    return !food_product_type.isEmpty() && target != 0;
  }
  [[nodiscard]] auto is_repair() const -> bool {
    return gather_product_type.isEmpty() && food_product_type.isEmpty() && target != 0;
  }
};

class ContextInteractionResolver {
public:
  ContextInteractionResolver(Engine::Core::World* world,
                             Render::GL::Camera* camera,
                             Game::Systems::PickingService* picking_service,
                             HoverTracker* hover_tracker);

  [[nodiscard]] auto resolve(qreal sx,
                             qreal sy,
                             const ViewportState& viewport) const -> ContextInteraction;

private:
  Engine::Core::World* m_world;
  Render::GL::Camera* m_camera;
  Game::Systems::PickingService* m_picking_service;
  HoverTracker* m_hover_tracker;
};
