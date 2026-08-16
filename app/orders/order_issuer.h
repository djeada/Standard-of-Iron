#pragma once

#include <QString>
#include <QVector3D>

#include <functional>

#include "app/orders/order_feedback.h"
#include "game/command/command.h"

namespace Engine::Core {
class World;
}

namespace App::Orders {

[[nodiscard]] auto local_owner(Engine::Core::World* world) -> int;

class OrderIssuer {
public:
  using FeedbackSink = std::function<void(const App::Core::OrderOutcome&)>;

  OrderIssuer(Engine::Core::World* world, FeedbackSink sink);

  [[nodiscard]] auto
  issue(App::Core::OrderKind kind,
        Game::Command::Payload payload,
        Engine::Core::EntityID target = 0,
        const QVector3D* destination = nullptr) -> App::Core::OrderOutcome;

  [[nodiscard]] auto reject(App::Core::OrderKind kind,
                            const QString& reason) -> App::Core::OrderOutcome;
  [[nodiscard]] auto reject_at(App::Core::OrderKind kind,
                               const QString& reason,
                               const QVector3D& destination) -> App::Core::OrderOutcome;
  [[nodiscard]] auto
  reject_on(App::Core::OrderKind kind,
            const QString& reason,
            Engine::Core::EntityID target) -> App::Core::OrderOutcome;

  auto publish(App::Core::OrderOutcome outcome) -> App::Core::OrderOutcome;

private:
  Engine::Core::World* m_world = nullptr;
  FeedbackSink m_sink;
};

} // namespace App::Orders
