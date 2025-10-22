#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>
#include <vector>

namespace Engine {
namespace Core {
class World;
class Entity;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine

namespace Game::Systems {
class SelectionSystem;
class PickingService;
} // namespace Game::Systems

namespace App::Controllers {

struct CommandResult {
  bool inputConsumed = false;
  bool resetCursorToNormal = false;
};

class CommandController : public QObject {
  Q_OBJECT
public:
  CommandController(Engine::Core::World *world,
                    Game::Systems::SelectionSystem *selectionSystem,
                    Game::Systems::PickingService *pickingService,
                    QObject *parent = nullptr);

  CommandResult onAttackClick(qreal sx, qreal sy, int viewportWidth,
                              int viewportHeight, void *camera);
  CommandResult onStopCommand();
  CommandResult onHoldCommand();
  CommandResult onPatrolClick(qreal sx, qreal sy, int viewportWidth,
                              int viewportHeight, void *camera);
  CommandResult setRallyAtScreen(qreal sx, qreal sy, int viewportWidth,
                                 int viewportHeight, void *camera,
                                 int localOwnerId);
  void recruitNearSelected(const QString &unitType, int localOwnerId);

  bool hasPatrolFirstWaypoint() const { return m_hasPatrolFirstWaypoint; }
  QVector3D getPatrolFirstWaypoint() const { return m_patrolFirstWaypoint; }
  void clearPatrolFirstWaypoint() { m_hasPatrolFirstWaypoint = false; }

  Q_INVOKABLE bool anySelectedInHoldMode() const;

signals:
  void attackTargetSelected();
  void troopLimitReached();
  void holdModeChanged(bool active);

private:
  Engine::Core::World *m_world;
  Game::Systems::SelectionSystem *m_selectionSystem;
  Game::Systems::PickingService *m_pickingService;

  bool m_hasPatrolFirstWaypoint = false;
  QVector3D m_patrolFirstWaypoint;

  void resetMovement(Engine::Core::Entity *entity);
};

} // namespace App::Controllers
