#pragma once

#include <QObject>
#include <QVector3D>
#include <QPointF>
#include <QString>
#include <vector>
#include <functional>

namespace Engine {
namespace Core {
class World;
class Entity;
using EntityID = unsigned int;
}
}

namespace Render {
namespace GL {
class Camera;
}
}

namespace Game {
namespace Systems {
class PickingService;
}
}

class QQuickWindow;

struct CommandResult {
  bool inputConsumed = false;
  bool resetCursorToNormal = false;
};

class CommandController : public QObject {
  Q_OBJECT
public:
  CommandController(Engine::Core::World *world,
                   Game::Systems::PickingService *pickingService,
                   QObject *parent = nullptr);

  CommandResult onAttackClick(qreal sx, qreal sy, QQuickWindow *window,
                             int viewportWidth, int viewportHeight,
                             int localOwnerId, Render::GL::Camera *camera);

  CommandResult onStopCommand();

  CommandResult onPatrolClick(qreal sx, qreal sy, QQuickWindow *window,
                             std::function<bool(const QPointF&, QVector3D&)> screenToGround);

  CommandResult setRallyAtScreen(qreal sx, qreal sy,
                                 std::function<bool(const QPointF&, QVector3D&)> screenToGround,
                                 int localOwnerId);

  CommandResult recruitNearSelected(const QString &unitType, int localOwnerId);

private:
  Engine::Core::World *m_world;
  Game::Systems::PickingService *m_pickingService;
  
  bool m_hasPatrolFirstWaypoint = false;
  QVector3D m_patrolFirstWaypoint;

  void resetMovement(Engine::Core::Entity *entity);
};
