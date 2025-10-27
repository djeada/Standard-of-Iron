#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector3D>
#include <limits>
#include <vector>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

class PickingService {
public:
  PickingService() = default;

  auto updateHover(float sx, float sy, Engine::Core::World &world,
                   const Render::GL::Camera &camera, int viewW,
                   int viewH) -> Engine::Core::EntityID;

  static auto screenToGround(const Render::GL::Camera &camera, int viewW,
                             int viewH, const QPointF &screenPt,
                             QVector3D &outWorld) -> bool;
  auto screenToGround(const QPointF &screenPt, const Render::GL::Camera &camera,
                      int viewW, int viewH, QVector3D &outWorld) const -> bool {
    return screenToGround(camera, viewW, viewH, screenPt, outWorld);
  }
  static auto worldToScreen(const Render::GL::Camera &camera, int viewW,
                            int viewH, const QVector3D &world,
                            QPointF &outScreen) -> bool;

  static auto pickSingle(float sx, float sy, Engine::Core::World &world,
                         const Render::GL::Camera &camera, int viewW, int viewH,
                         int ownerFilter,
                         bool preferBuildingsFirst) -> Engine::Core::EntityID;

  auto pickUnitFirst(float sx, float sy, Engine::Core::World &world,
                     const Render::GL::Camera &camera, int viewW, int viewH,
                     int ownerFilter) const -> Engine::Core::EntityID;

  static auto
  pickInRect(float x1, float y1, float x2, float y2, Engine::Core::World &world,
             const Render::GL::Camera &camera, int viewW, int viewH,
             int ownerFilter) -> std::vector<Engine::Core::EntityID>;

private:
  Engine::Core::EntityID m_prev_hoverId = 0;
  int m_hoverGraceTicks = 0;
  auto projectBounds(const Render::GL::Camera &cam, const QVector3D &center,
                     float hx, float hz, int viewW, int viewH,
                     QRectF &out) const -> bool;
};

} // namespace Game::Systems
