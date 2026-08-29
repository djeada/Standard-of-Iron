#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "app/orders/army_formation_controller.h"
#include "app/orders/order_feedback.h"
#include "app/orders/order_issuer.h"
#include "game/command/command.h"

namespace Engine::Core {
class World;
class Entity;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {
class SelectionSystem;
class PickingService;
} // namespace Game::Systems

namespace App::Controllers {

struct CommandResult {
  bool input_consumed = false;
  bool reset_cursor_to_normal = false;

  App::Core::OrderOutcome order;
};

class CommandController : public QObject {
  Q_OBJECT
public:
  CommandController(Engine::Core::World* world,
                    Game::Systems::SelectionSystem* selection_system,
                    Game::Systems::PickingService* picking_service,
                    QObject* parent = nullptr);

  auto on_attack_click(qreal sx,
                       qreal sy,
                       int viewport_width,
                       int viewport_height,
                       void* camera) -> CommandResult;

  auto on_attack_press(qreal sx,
                       qreal sy,
                       int viewport_width,
                       int viewport_height,
                       void* camera,
                       int local_owner_id) -> CommandResult;

  auto on_move_or_attack_click(qreal sx,
                               qreal sy,
                               int viewport_width,
                               int viewport_height,
                               void* camera,
                               int local_owner_id) -> CommandResult;

  auto on_minimap_move(const QVector3D& world_target,
                       int local_owner_id) -> CommandResult;

  auto on_stop_command() -> CommandResult;
  auto on_hold_command() -> CommandResult;
  auto on_gate_command() -> CommandResult;
  auto on_guard_command() -> CommandResult;

  auto
  on_auto_gather_command(const QString& priority_product_type = {}) -> CommandResult;
  [[nodiscard]] auto divide_selected_squads() -> CommandResult;

  [[nodiscard]] auto merge_selected_squads() -> CommandResult;

  [[nodiscard]] auto
  set_auto_gather(bool active,
                  const QString& priority_product_type = {}) -> CommandResult;
  auto on_run_command() -> CommandResult;

  auto start_food_harvest(Engine::Core::EntityID target,
                          const QString& product_type,
                          int local_owner_id) -> CommandResult;
  void enable_run_mode_for_selected();
  void disable_run_mode_for_selected();
  auto on_guard_click(qreal sx,
                      qreal sy,
                      int viewport_width,
                      int viewport_height,
                      void* camera) -> CommandResult;
  auto on_civilian_delivery_click(qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  void* camera,
                                  int local_owner_id) -> CommandResult;
  auto on_builder_repair_click(qreal sx,
                               qreal sy,
                               int viewport_width,
                               int viewport_height,
                               void* camera,
                               int local_owner_id) -> CommandResult;
  auto on_builder_dismantle_click(qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  void* camera,
                                  int local_owner_id) -> CommandResult;
  auto on_patrol_click(qreal sx,
                       qreal sy,
                       int viewport_width,
                       int viewport_height,
                       void* camera) -> CommandResult;
  auto set_rally_at_screen(qreal sx,
                           qreal sy,
                           int viewport_width,
                           int viewport_height,
                           void* camera,
                           int local_owner_id) -> CommandResult;
  void recruit_near_selected(const QString& unit_type, int local_owner_id);

  [[nodiscard]] bool has_patrol_first_waypoint() const {
    return m_has_patrol_first_waypoint;
  }
  [[nodiscard]] QVector3D get_patrol_first_waypoint() const {
    return m_patrol_first_waypoint;
  }
  void clear_patrol_first_waypoint() { m_has_patrol_first_waypoint = false; }
  void reset_transient_state();

  [[nodiscard]] auto formation() -> ArmyFormationController& { return m_formation; }
  [[nodiscard]] auto formation() const -> const ArmyFormationController& {
    return m_formation;
  }

  Q_INVOKABLE [[nodiscard]] bool any_selected_in_hold_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_guard_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_run_mode() const;

signals:
  void order_feedback(const App::Core::OrderOutcome& outcome);
  void troop_limit_reached();
  void insufficient_manpower();
  void insufficient_resources(const QString& message);
  void hold_mode_changed(bool active);
  void gate_mode_changed(const QString& mode);
  void guard_mode_changed(bool active);
  void formation_mode_changed(bool active);
  void run_mode_changed(bool active);
  void auto_gather_changed(bool active);
  void formation_placement_started();
  void formation_placement_updated(QVector3D position, float angle);
  void formation_placement_ended();
  void formation_deployed(int unit_count);
  void formation_placement_rejected(const QString& reason);
  void formation_preview_changed();

private:
  auto issue_auto_gather(const std::vector<Engine::Core::EntityID>& builders,
                         bool active,
                         const QString& priority_product_type) -> CommandResult;

  Engine::Core::World* m_world;
  Game::Systems::SelectionSystem* m_selection_system;
  Game::Systems::PickingService* m_picking_service;

  bool m_has_patrol_first_waypoint = false;
  QVector3D m_patrol_first_waypoint;

  App::Orders::OrderIssuer m_orders;
  ArmyFormationController m_formation;
};

} // namespace App::Controllers
