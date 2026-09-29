#pragma once

#include <QObject>
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

namespace Game::Systems {
class PickingService;
}

namespace App::Controllers {

class WorkerCommands : public QObject {
  Q_OBJECT
public:
  WorkerCommands(Engine::Core::World* world,
                 Game::Session::SelectionService* selection,
                 Game::Systems::PickingService* picking,
                 App::Orders::OrderIssuer& orders,
                 QObject* parent = nullptr);

  auto on_auto_gather_command(const QString& priority_product_type) -> CommandResult;
  auto set_auto_gather(bool active,
                       const QString& priority_product_type) -> CommandResult;
  auto start_food_harvest(Engine::Core::EntityID target,
                          const QString& product_type,
                          int local_owner_id) -> CommandResult;
  auto on_civilian_delivery_click(const PointerTarget& target,
                                  int local_owner_id) -> CommandResult;
  auto on_builder_repair_click(const PointerTarget& target,
                               int local_owner_id) -> CommandResult;
  auto on_builder_dismantle_click(const PointerTarget& target,
                                  int local_owner_id) -> CommandResult;

signals:
  void auto_gather_changed(bool active);

private:
  enum class CursorReset : std::uint8_t {
    Always,
    WhenAccepted
  };

  template <class Issue>
  auto pointer_order(const PointerTarget& target,
                     App::Core::OrderKind empty_selection_kind,
                     CursorReset reset,
                     Issue&& issue) -> CommandResult;
  [[nodiscard]] auto eligible_gatherers() const -> std::vector<Engine::Core::EntityID>;
  auto reject_no_gatherers() -> CommandResult;
  auto issue_auto_gather(const std::vector<Engine::Core::EntityID>& builders,
                         bool active,
                         const QString& priority_product_type) -> CommandResult;

  Engine::Core::World* m_world;
  Game::Session::SelectionService* m_selection;
  Game::Systems::PickingService* m_picking;
  App::Orders::OrderIssuer& m_orders;
};

} // namespace App::Controllers
