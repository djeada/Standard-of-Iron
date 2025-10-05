#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector3D>
#include <limits>
#include <vector>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine

namespace Render {
namespace GL {
class Camera;
}
} // namespace Render

namespace Game {
namespace Systems {

class PickingService {
public:
  PickingService() = default;

  Engine::Core::EntityID updateHover(float sx, float sy,
                                     Engine::Core::World &world,
                                     const Render::GL::Camera &camera,
                                     int viewW, int viewH);

  bool screenToGround(const Render::GL::Camera &camera, int viewW, int viewH,
                      const QPointF &screenPt, QVector3D &outWorld) const;
  bool worldToScreen(const Render::GL::Camera &camera, int viewW, int viewH,
                     const QVector3D &world, QPointF &outScreen) const;

  Engine::Core::EntityID pickSingle(float sx, float sy,
                                    Engine::Core::World &world,
                                    const Render::GL::Camera &camera, int viewW,
                                    int viewH, int ownerFilter,
                                    bool preferBuildingsFirst) const;

  std::vector<Engine::Core::EntityID>
  pickInRect(float x1, float y1, float x2, float y2, Engine::Core::World &world,
             const Render::GL::Camera &camera, int viewW, int viewH,
             int ownerFilter) const;

private:
  Engine::Core::EntityID m_prevHoverId = 0;
  int m_hoverGraceTicks = 0;
  bool projectBounds(const Render::GL::Camera &cam, const QVector3D &center,
                     float hx, float hz, int viewW, int viewH,
                     QRectF &out) const;
};

} // namespace Systems
} // namespace Game
