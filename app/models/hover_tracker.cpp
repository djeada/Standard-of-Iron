#include "hover_tracker.h"
#include "game/core/world.h"
#include "render/gl/camera.h"
#include "systems/picking_service.h"

HoverTracker::HoverTracker(Game::Systems::PickingService *pickingService)
    : m_pickingService(pickingService) {}

auto HoverTracker::updateHover(float sx, float sy, Engine::Core::World &world,
                               const Render::GL::Camera &camera,
                               int viewportWidth,
                               int viewportHeight) -> Engine::Core::EntityID {
  if (m_pickingService == nullptr) {
    return 0;
  }

  if (sx < 0 || sy < 0 || sx >= viewportWidth || sy >= viewportHeight) {
    m_hoveredEntityId = 0;
    return 0;
  }

  m_hoveredEntityId = m_pickingService->updateHover(
      sx, sy, world, camera, viewportWidth, viewportHeight);

  return m_hoveredEntityId;
}
