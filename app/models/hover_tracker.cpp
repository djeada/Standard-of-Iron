#include "hover_tracker.h"

#include "game/core/world.h"
#include "render/gl/camera.h"
#include "systems/picking_service.h"

HoverTracker::HoverTracker(Game::Systems::PickingService* picking_service)
    : m_picking_service(picking_service) {
}

auto HoverTracker::update_hover(float sx,
                                float sy,
                                Engine::Core::World& world,
                                const Render::GL::Camera& camera,
                                int viewport_width,
                                int viewport_height) -> Engine::Core::EntityID {
  if (m_picking_service == nullptr) {
    return 0;
  }

  if (sx < 0 || sy < 0 || sx >= viewport_width || sy >= viewport_height) {
    m_hovered_entity_id = 0;
    return 0;
  }

  m_hovered_entity_id = m_picking_service->update_hover(
      sx, sy, world, camera, viewport_width, viewport_height);

  return m_hovered_entity_id;
}
