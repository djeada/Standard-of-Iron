#pragma once

#include "game/systems/picking_service.h"
#include "render/gl/camera.h"
#include <QPointF>
#include <QQuickWindow>
#include <QVector3D>

namespace App::Utils {

inline auto
screen_to_ground(const Game::Systems::PickingService *picking_service,
                 const Render::GL::Camera *camera, QQuickWindow *window,
                 int viewport_width, int viewport_height,
                 const QPointF &screen_pt, QVector3D &out_world) -> bool {
  if ((window == nullptr) || (camera == nullptr) ||
      (picking_service == nullptr)) {
    return false;
  }
  int const w = (viewport_width > 0 ? viewport_width : window->width());
  int const h = (viewport_height > 0 ? viewport_height : window->height());
  return Game::Systems::PickingService::screen_to_ground(*camera, w, h,
                                                         screen_pt, out_world);
}

inline auto
world_to_screen(const Game::Systems::PickingService *picking_service,
                const Render::GL::Camera *camera, QQuickWindow *window,
                int viewport_width, int viewport_height, const QVector3D &world,
                QPointF &out_screen) -> bool {
  if ((window == nullptr) || (camera == nullptr) ||
      (picking_service == nullptr)) {
    return false;
  }
  int const w = (viewport_width > 0 ? viewport_width : window->width());
  int const h = (viewport_height > 0 ? viewport_height : window->height());
  return Game::Systems::PickingService::world_to_screen(*camera, w, h, world,
                                                        out_screen);
}

} // namespace App::Utils
