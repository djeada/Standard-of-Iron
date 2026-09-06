#include "app/orders/order_submission.h"

#include <algorithm>
#include <type_traits>
#include <utility>
#include <variant>

#include "game/command/command_queue.h"
#include "game/command/command_validator.h"
#include "game/core/component_economy.h"
#include "game/core/world.h"

namespace App::Core {

namespace {

auto payload_units(const Game::Command::Payload& payload)
    -> const std::vector<Engine::Core::EntityID>* {
  return std::visit(
      [](const auto& body) -> const std::vector<Engine::Core::EntityID>* {
        if constexpr (requires { body.units; }) {
          return &body.units;
        } else {
          return nullptr;
        }
      },
      payload);
}

auto is_hauling_a_load(const Engine::Core::World& world,
                       Engine::Core::EntityID unit) -> bool {
  const auto* carry = world.try_get<Engine::Core::ResourceCarryComponent>(unit);
  return carry != nullptr && !carry->empty();
}

auto every_unit_is_hauling(const Engine::Core::World& world,
                           const Game::Command::Payload& payload) -> bool {
  const auto* units = payload_units(payload);
  if (units == nullptr || units->empty()) {
    return false;
  }
  return std::all_of(units->begin(), units->end(), [&world](auto id) {
    return is_hauling_a_load(world, id);
  });
}

} // namespace

auto payload_unit_count(const Game::Command::Payload& payload) -> std::size_t {
  return std::visit(
      [](const auto& body) -> std::size_t {
        using T = std::decay_t<decltype(body)>;
        if constexpr (requires { body.units; }) {
          return body.units.size();
        } else if constexpr (std::is_same_v<T, Game::Command::UseCommanderAbility>) {
          return body.commander != Engine::Core::NULL_ENTITY ? 1U : 0U;
        } else if constexpr (std::is_same_v<T, Game::Command::SetRallyPoint> ||
                             std::is_same_v<T, Game::Command::Produce>) {
          return body.building != Engine::Core::NULL_ENTITY ? 1U : 0U;
        } else {
          return 0U;
        }
      },
      payload);
}

auto submit_player_order(Engine::Core::World& world,
                         int owner_id,
                         OrderRequest request) -> OrderOutcome {
  OrderOutcome outcome;
  outcome.kind = request.kind;
  outcome.target = request.target;
  outcome.has_destination = request.has_destination;
  outcome.destination = request.destination;

  if (every_unit_is_hauling(world, request.payload)) {
    outcome.unit_count = payload_unit_count(request.payload);
    outcome.status = OrderStatus::Rejected;
    auto refusal = hauling_load_reason();
    outcome.failure = refusal.failure;
    outcome.reason = std::move(refusal.text);
    return outcome;
  }

  const Game::Command::Command command{.source = Game::Command::Source::LocalPlayer,
                                       .owner_id = owner_id,
                                       .payload = std::move(request.payload)};

  auto validation = Game::Command::validate(world, command);
  outcome.rejection = validation.rejection;
  outcome.unit_count = payload_unit_count(validation.command.payload);
  if (!validation.accepted()) {
    outcome.status = OrderStatus::Rejected;
    outcome.failure = failure_for(validation.rejection);
    outcome.reason = rejection_reason_text(validation.rejection, request.kind);
    return outcome;
  }

  Game::Command::submit(world,
                        Game::Command::Source::LocalPlayer,
                        owner_id,
                        std::move(validation.command.payload));
  outcome.status = OrderStatus::Accepted;
  return outcome;
}

auto rejected_order(OrderKind kind, OrderRefusal refusal) -> OrderOutcome {
  OrderOutcome outcome;
  outcome.kind = kind;
  outcome.status = OrderStatus::Rejected;
  outcome.failure = refusal.failure;
  outcome.reason = std::move(refusal.text);
  return outcome;
}

auto rejected_order_at(OrderKind kind,
                       OrderRefusal refusal,
                       const QVector3D& destination) -> OrderOutcome {
  auto outcome = rejected_order(kind, std::move(refusal));
  outcome.has_destination = true;
  outcome.destination = destination;
  return outcome;
}

auto rejected_order_on(OrderKind kind,
                       OrderRefusal refusal,
                       Engine::Core::EntityID target) -> OrderOutcome {
  auto outcome = rejected_order(kind, std::move(refusal));
  outcome.target = target;
  return outcome;
}

} // namespace App::Core
