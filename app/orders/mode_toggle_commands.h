#pragma once

#include <QObject>
#include <QString>

#include "app/orders/command_result.h"
#include "app/orders/order_issuer.h"

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SelectionService;
}

namespace App::Controllers {

class ModeToggleCommands : public QObject {
  Q_OBJECT
public:
  ModeToggleCommands(Engine::Core::World* world,
                     Game::Session::SelectionService* selection,
                     App::Orders::OrderIssuer& orders,
                     QObject* parent = nullptr);

  auto on_stop_command() -> CommandResult;
  auto on_hold_command() -> CommandResult;
  auto on_guard_command() -> CommandResult;
  auto on_guard_click(const PointerTarget& target) -> CommandResult;
  auto on_gate_command() -> CommandResult;
  auto on_run_command() -> CommandResult;
  void enable_run_mode_for_selected();
  void disable_run_mode_for_selected();

  [[nodiscard]] auto any_selected_in_hold_mode() const -> bool;
  [[nodiscard]] auto any_selected_in_guard_mode() const -> bool;
  [[nodiscard]] auto any_selected_in_run_mode() const -> bool;

signals:
  void hold_mode_changed(bool active);
  void gate_mode_changed(const QString& mode);
  void guard_mode_changed(bool active);
  void formation_mode_changed(bool active);
  void run_mode_changed(bool active);

private:
  [[nodiscard]] auto ready() const -> bool;

  Engine::Core::World* m_world;
  Game::Session::SelectionService* m_selection;
  App::Orders::OrderIssuer& m_orders;
};

} // namespace App::Controllers
