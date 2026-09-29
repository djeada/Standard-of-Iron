#pragma once

#include <QVector3D>

#include "app/orders/command_result.h"
#include "app/orders/order_issuer.h"

namespace Engine::Core {
class World;
}

namespace Game::Session {
class SelectionService;
}

namespace Game::Systems {
class PickingService;
}

namespace App::Controllers {

class PatrolCommands {
public:
  PatrolCommands(Engine::Core::World* world,
                 Game::Session::SelectionService* selection,
                 Game::Systems::PickingService* picking,
                 App::Orders::OrderIssuer& orders);

  auto on_patrol_click(const PointerTarget& target) -> CommandResult;

  [[nodiscard]] auto has_first_waypoint() const -> bool { return m_has_first_waypoint; }
  [[nodiscard]] auto first_waypoint() const -> QVector3D { return m_first_waypoint; }
  void clear_first_waypoint() { m_has_first_waypoint = false; }
  void reset();

private:
  Engine::Core::World* m_world;
  Game::Session::SelectionService* m_selection;
  Game::Systems::PickingService* m_picking;
  App::Orders::OrderIssuer& m_orders;

  bool m_has_first_waypoint = false;
  QVector3D m_first_waypoint;
};

} // namespace App::Controllers
