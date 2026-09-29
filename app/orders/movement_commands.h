#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

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

class ModeToggleCommands;

class MovementCommands {
public:
  MovementCommands(Engine::Core::World* world,
                   Game::Session::SelectionService* selection,
                   Game::Systems::PickingService* picking,
                   App::Orders::OrderIssuer& orders,
                   ModeToggleCommands& modes);

  auto on_attack_click(const PointerTarget& target,
                       int local_owner_id) -> CommandResult;
  auto on_attack_press(const PointerTarget& target,
                       int local_owner_id) -> CommandResult;
  auto on_move_or_attack_click(const PointerTarget& target,
                               int local_owner_id) -> CommandResult;
  auto on_minimap_move(const QVector3D& world_target,
                       int local_owner_id) -> CommandResult;
  auto refuse_unreachable_move(const QVector3D& destination) -> App::Core::OrderOutcome;

private:
  [[nodiscard]] auto ready_for_pointer(const PointerTarget& target) const -> bool;
  auto attack_ground(const PointerTarget& target,
                     const std::vector<Engine::Core::EntityID>& selected,
                     int local_owner_id) -> CommandResult;
  auto
  attack_unit(Engine::Core::EntityID target_id,
              const std::vector<Engine::Core::EntityID>& selected) -> CommandResult;

  Engine::Core::World* m_world;
  Game::Session::SelectionService* m_selection;
  Game::Systems::PickingService* m_picking;
  App::Orders::OrderIssuer& m_orders;
  ModeToggleCommands& m_modes;
};

} // namespace App::Controllers
