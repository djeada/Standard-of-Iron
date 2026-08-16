#pragma once

#include <QString>
#include <QVector3D>

#include <cstddef>

#include "app/orders/order_feedback.h"
#include "game/command/command.h"

namespace Engine::Core {
class World;
}

namespace App::Core {

struct OrderRequest {
  OrderKind kind = OrderKind::None;
  Game::Command::Payload payload;

  Engine::Core::EntityID target = 0;

  bool has_destination = false;
  QVector3D destination;
};

[[nodiscard]] auto submit_player_order(Engine::Core::World& world,
                                       int owner_id,
                                       OrderRequest request) -> OrderOutcome;

[[nodiscard]] auto rejected_order(OrderKind kind, QString reason) -> OrderOutcome;

[[nodiscard]] auto rejected_order_at(OrderKind kind,
                                     QString reason,
                                     const QVector3D& destination) -> OrderOutcome;

[[nodiscard]] auto rejected_order_on(OrderKind kind,
                                     QString reason,
                                     Engine::Core::EntityID target) -> OrderOutcome;

[[nodiscard]] auto
payload_unit_count(const Game::Command::Payload& payload) -> std::size_t;

} // namespace App::Core
