#pragma once

#include "game/systems/picking_service.h"
#include "render/gl/camera.h"
#include <QPointF>
#include <QQuickWindow>
#include <QVector3D>

namespace App {
namespace Utils {

inline bool screenToGround(const Game::Systems::PickingService *pickingService,
                           const Render::GL::Camera *camera,
                           QQuickWindow *window, int viewportWidth,
                           int viewportHeight, const QPointF &screenPt,
                           QVector3D &outWorld) {
  if (!window || !camera || !pickingService)
    return false;
  int w = (viewportWidth > 0 ? viewportWidth : window->width());
  int h = (viewportHeight > 0 ? viewportHeight : window->height());
  return pickingService->screenToGround(*camera, w, h, screenPt, outWorld);
}

inline bool worldToScreen(const Game::Systems::PickingService *pickingService,
                          const Render::GL::Camera *camera,
                          QQuickWindow *window, int viewportWidth,
                          int viewportHeight, const QVector3D &world,
                          QPointF &outScreen) {
  if (!window || !camera || !pickingService)
    return false;
  int w = (viewportWidth > 0 ? viewportWidth : window->width());
  int h = (viewportHeight > 0 ? viewportHeight : window->height());
  return pickingService->worldToScreen(*camera, w, h, world, outScreen);
}

} 
} 
