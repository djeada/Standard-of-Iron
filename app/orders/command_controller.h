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
#include "app/orders/command_result.h"
#include "app/orders/mode_toggle_commands.h"
#include "app/orders/movement_commands.h"
#include "app/orders/order_feedback.h"
#include "app/orders/order_issuer.h"
#include "app/orders/patrol_commands.h"
#include "app/orders/roster_commands.h"
#include "app/orders/worker_commands.h"
#include "game/audio/cue_ids.h"
#include "game/command/command.h"
#include "game/units/troop_type.h"

namespace Engine::Core {
class World;
class Entity;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {
class PickingService;
} // namespace Game::Systems

namespace App::Controllers {

class CommandController : public QObject {
  Q_OBJECT
public:
  CommandController(Engine::Core::World* world,
                    Game::Session::SelectionService* selection_system,
                    Game::Systems::PickingService* picking_service,
                    QObject* parent = nullptr);

  auto on_attack_click(qreal sx,
                       qreal sy,
                       int viewport_width,
                       int viewport_height,
                       void* camera,
                       int local_owner_id) -> CommandResult;

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

  [[nodiscard]] static auto
  selection_mounts(Engine::Core::World& world,
                   const std::vector<Engine::Core::EntityID>& units)
      -> Game::Audio::Cue::SelectionMounts;
  [[nodiscard]] static auto
  charge_cue(const Game::Audio::Cue::SelectionMounts& mounts) -> const char*;
  [[nodiscard]] static auto
  move_order_cue(const Game::Audio::Cue::SelectionMounts& mounts) -> const char*;
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
  void recruit_near_selected(const QString& unit_type, int local_owner_id);

  [[nodiscard]] bool has_patrol_first_waypoint() const {
    return m_patrol.has_first_waypoint();
  }
  [[nodiscard]] QVector3D get_patrol_first_waypoint() const {
    return m_patrol.first_waypoint();
  }
  void clear_patrol_first_waypoint() { m_patrol.clear_first_waypoint(); }
  void reset_transient_state();

  auto refuse_unreachable_move(const QVector3D& destination) -> App::Core::OrderOutcome;

  [[nodiscard]] auto formation() -> ArmyFormationController& { return m_formation; }
  [[nodiscard]] auto formation() const -> const ArmyFormationController& {
    return m_formation;
  }

  Q_INVOKABLE [[nodiscard]] bool any_selected_in_hold_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_guard_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_run_mode() const;

signals:
  void order_feedback(const App::Core::OrderOutcome& outcome);
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
  [[nodiscard]] static auto target_at(qreal sx,
                                      qreal sy,
                                      int viewport_width,
                                      int viewport_height,
                                      void* camera) -> PointerTarget;

  Game::Systems::PickingService* m_picking_service;

  App::Orders::OrderIssuer m_orders;
  ArmyFormationController m_formation;
  ModeToggleCommands m_modes;
  MovementCommands m_movement;
  WorkerCommands m_workers;
  RosterCommands m_roster;
  PatrolCommands m_patrol;
};

} // namespace App::Controllers
