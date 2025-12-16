#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>
#include <vector>

namespace Engine::Core {
class World;
class Entity;
using EntityID = unsigned int;
} // namespace Engine::Core

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
                    Game::Systems::SelectionSystem *selection_system,
                    Game::Systems::PickingService *pickingService,
                    QObject *parent = nullptr);

  auto onAttackClick(qreal sx, qreal sy, int viewportWidth, int viewportHeight,
                     void *camera) -> CommandResult;
  auto onStopCommand() -> CommandResult;
  auto onHoldCommand() -> CommandResult;
  auto onGuardCommand() -> CommandResult;
  auto onGuardClick(qreal sx, qreal sy, int viewportWidth, int viewportHeight,
                    void *camera) -> CommandResult;
  auto onPatrolClick(qreal sx, qreal sy, int viewportWidth, int viewportHeight,
                     void *camera) -> CommandResult;
  auto setRallyAtScreen(qreal sx, qreal sy, int viewportWidth,
                        int viewportHeight, void *camera,
                        int local_owner_id) -> CommandResult;
  void recruitNearSelected(const QString &unit_type, int local_owner_id);

  [[nodiscard]] bool hasPatrolFirstWaypoint() const {
    return m_hasPatrolFirstWaypoint;
  }
  [[nodiscard]] QVector3D getPatrolFirstWaypoint() const {
    return m_patrolFirstWaypoint;
  }
  void clearPatrolFirstWaypoint() { m_hasPatrolFirstWaypoint = false; }

  Q_INVOKABLE [[nodiscard]] bool anySelectedInHoldMode() const;
  Q_INVOKABLE [[nodiscard]] bool anySelectedInGuardMode() const;

signals:
  void attack_targetSelected();
  void troopLimitReached();
  void hold_modeChanged(bool active);
  void guard_modeChanged(bool active);

private:
  Engine::Core::World *m_world;
  Game::Systems::SelectionSystem *m_selection_system;
  Game::Systems::PickingService *m_pickingService;

  bool m_hasPatrolFirstWaypoint = false;
  QVector3D m_patrolFirstWaypoint;

  static void resetMovement(Engine::Core::Entity *entity);
};

} // namespace App::Controllers
