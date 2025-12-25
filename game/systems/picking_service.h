#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector3D>
#include <limits>
#include <vector>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} 

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

class PickingService {
public:
  PickingService() = default;

  auto update_hover(float sx, float sy, Engine::Core::World &world,
                    const Render::GL::Camera &camera, int view_w,
                    int view_h) -> Engine::Core::EntityID;

  static auto screen_to_ground(const Render::GL::Camera &camera, int view_w,
                               int view_h, const QPointF &screen_pt,
                               QVector3D &out_world) -> bool;
  static auto screen_to_ground(const QPointF &screen_pt,
                               const Render::GL::Camera &camera, int view_w,
                               int view_h, QVector3D &out_world) -> bool {
    return screen_to_ground(camera, view_w, view_h, screen_pt, out_world);
  }
  static auto world_to_screen(const Render::GL::Camera &camera, int view_w,
                              int view_h, const QVector3D &world,
                              QPointF &out_screen) -> bool;

  static auto
  pick_single(float sx, float sy, Engine::Core::World &world,
              const Render::GL::Camera &camera, int view_w, int view_h,
              int owner_filter,
              bool prefer_buildings_first) -> Engine::Core::EntityID;

  static auto pick_unit_first(float sx, float sy, Engine::Core::World &world,
                              const Render::GL::Camera &camera, int view_w,
                              int view_h,
                              int owner_filter) -> Engine::Core::EntityID;

  static auto
  pick_in_rect(float x1, float y1, float x2, float y2,
               Engine::Core::World &world, const Render::GL::Camera &camera,
               int view_w, int view_h,
               int owner_filter) -> std::vector<Engine::Core::EntityID>;

private:
  Engine::Core::EntityID m_prev_hoverId = 0;
  int m_hover_grace_ticks = 0;
  static auto project_bounds(const Render::GL::Camera &cam,
                             const QVector3D &center, float hx, float hz,
                             int view_w, int view_h, QRectF &out) -> bool;
};

} 
