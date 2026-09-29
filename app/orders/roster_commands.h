#pragma once

#include <QString>

#include <cstdint>

#include "app/orders/command_result.h"
#include "app/orders/order_issuer.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Session {
class SelectionService;
}

namespace App::Controllers {

class RosterCommands {
public:
  RosterCommands(Engine::Core::World* world,
                 Game::Session::SelectionService* selection,
                 App::Orders::OrderIssuer& orders);

  void recruit_near_selected(const QString& unit_type, int local_owner_id);
  [[nodiscard]] auto divide_selected_squads() -> CommandResult;
  [[nodiscard]] auto merge_selected_squads() -> CommandResult;

private:
  Engine::Core::World* m_world;
  Game::Session::SelectionService* m_selection;
  App::Orders::OrderIssuer& m_orders;
};

} // namespace App::Controllers
