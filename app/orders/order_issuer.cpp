#include "app/orders/order_issuer.h"

#include <utility>

#include "app/orders/order_submission.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"

namespace App::Orders {

auto local_owner(Engine::Core::World* world) -> int {
  return Game::Session::session_for(*world).owners().get_local_player_id();
}

OrderIssuer::OrderIssuer(Engine::Core::World* world, FeedbackSink sink)
    : m_world(world)
    , m_sink(std::move(sink)) {
}

auto OrderIssuer::publish(App::Core::OrderOutcome outcome) -> App::Core::OrderOutcome {
  if (outcome.issued() && m_sink) {
    m_sink(outcome);
  }
  return outcome;
}

auto OrderIssuer::issue(App::Core::OrderKind kind,
                        Game::Command::Payload payload,
                        Engine::Core::EntityID target,
                        const QVector3D* destination) -> App::Core::OrderOutcome {
  if (m_world == nullptr) {
    return publish(App::Core::rejected_order(
        kind,
        App::Core::rejection_refusal(Game::Command::Rejection::MalformedPayload,
                                     kind)));
  }

  App::Core::OrderRequest request;
  request.kind = kind;
  request.payload = std::move(payload);
  request.target = target;
  if (destination != nullptr) {
    request.has_destination = true;
    request.destination = *destination;
  }
  return publish(App::Core::submit_player_order(
      *m_world, local_owner(m_world), std::move(request)));
}

auto OrderIssuer::reject(App::Core::OrderKind kind,
                         App::Core::OrderRefusal refusal) -> App::Core::OrderOutcome {
  return publish(App::Core::rejected_order(kind, std::move(refusal)));
}

auto OrderIssuer::reject_at(App::Core::OrderKind kind,
                            App::Core::OrderRefusal refusal,
                            const QVector3D& destination) -> App::Core::OrderOutcome {
  return publish(App::Core::rejected_order_at(kind, std::move(refusal), destination));
}

auto OrderIssuer::reject_on(App::Core::OrderKind kind,
                            App::Core::OrderRefusal refusal,
                            Engine::Core::EntityID target) -> App::Core::OrderOutcome {
  return publish(App::Core::rejected_order_on(kind, std::move(refusal), target));
}

} // namespace App::Orders
