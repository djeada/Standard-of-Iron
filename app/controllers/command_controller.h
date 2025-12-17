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
  bool input_consumed = false;
  bool reset_cursor_to_normal = false;
};

class CommandController : public QObject {
  Q_OBJECT
public:
  CommandController(Engine::Core::World *world,
                    Game::Systems::SelectionSystem *selection_system,
                    Game::Systems::PickingService *picking_service,
                    QObject *parent = nullptr);

  auto on_attack_click(qreal sx, qreal sy, int viewport_width,
                       int viewport_height, void *camera) -> CommandResult;
  auto on_stop_command() -> CommandResult;
  auto on_hold_command() -> CommandResult;
  auto on_guard_command() -> CommandResult;
  auto on_formation_command() -> CommandResult;
  auto on_run_command() -> CommandResult;
  auto on_guard_click(qreal sx, qreal sy, int viewport_width,
                      int viewport_height, void *camera) -> CommandResult;
  auto on_patrol_click(qreal sx, qreal sy, int viewport_width,
                       int viewport_height, void *camera) -> CommandResult;
  auto set_rally_at_screen(qreal sx, qreal sy, int viewport_width,
                           int viewport_height, void *camera,
                           int local_owner_id) -> CommandResult;
  void recruit_near_selected(const QString &unit_type, int local_owner_id);

  [[nodiscard]] bool has_patrol_first_waypoint() const {
    return m_has_patrol_first_waypoint;
  }
  [[nodiscard]] QVector3D get_patrol_first_waypoint() const {
    return m_patrol_first_waypoint;
  }
  void clear_patrol_first_waypoint() { m_has_patrol_first_waypoint = false; }

  Q_INVOKABLE [[nodiscard]] bool any_selected_in_hold_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_guard_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_formation_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_run_mode() const;

signals:
  void attack_target_selected();
  void troop_limit_reached();
  void hold_mode_changed(bool active);
  void guard_mode_changed(bool active);
  void formation_mode_changed(bool active);
  void run_mode_changed(bool active);

private:
  Engine::Core::World *m_world;
  Game::Systems::SelectionSystem *m_selection_system;
  Game::Systems::PickingService *m_picking_service;

  bool m_has_patrol_first_waypoint = false;
  QVector3D m_patrol_first_waypoint;

  static void reset_movement(Engine::Core::Entity *entity);
};

} // namespace App::Controllers
