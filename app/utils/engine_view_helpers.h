#pragma once

#include "game/systems/picking_service.h"
#include "render/gl/camera.h"
#include <QPointF>
#include <QQuickWindow>
#include <QVector3D>

namespace App::Utils {

inline auto screenToGround(const Game::Systems::PickingService *pickingService,
                           const Render::GL::Camera *camera,
                           QQuickWindow *window, int viewportWidth,
                           int viewportHeight, const QPointF &screenPt,
                           QVector3D &outWorld) -> bool {
  if ((window == nullptr) || (camera == nullptr) ||
      (pickingService == nullptr)) {
    return false;
  }
  int const w = (viewportWidth > 0 ? viewportWidth : window->width());
  int const h = (viewportHeight > 0 ? viewportHeight : window->height());
  return pickingService->screenToGround(*camera, w, h, screenPt, outWorld);
}

inline auto worldToScreen(const Game::Systems::PickingService *pickingService,
                          const Render::GL::Camera *camera,
                          QQuickWindow *window, int viewportWidth,
                          int viewportHeight, const QVector3D &world,
                          QPointF &outScreen) -> bool {
  if ((window == nullptr) || (camera == nullptr) ||
      (pickingService == nullptr)) {
    return false;
  }
  int const w = (viewportWidth > 0 ? viewportWidth : window->width());
  int const h = (viewportHeight > 0 ? viewportHeight : window->height());
  return pickingService->worldToScreen(*camera, w, h, world, outScreen);
}

} // namespace App::Utils
