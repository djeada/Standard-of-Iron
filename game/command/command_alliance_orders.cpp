#include "../session/session_context.h"
#include "../systems/alliance_board.h"
#include "../systems/economy/marketplace_system.h"
#include "command_handlers.h"

namespace Game::Command::handlers {

using Engine::Core::Entity;
using Engine::Core::EntityID;
using Engine::Core::World;

void apply_trade(World& world, int owner_id, const Trade& trade) {
  auto& marketplace = Game::Session::session_for(world).marketplace();
  if (trade.direction == TradeDirection::Buy) {
    static_cast<void>(marketplace.buy_resource(world, owner_id, trade.resource));
  } else {
    static_cast<void>(marketplace.sell_resource(world, owner_id, trade.resource));
  }
}
void apply_ally_tribute(World& world, int owner_id, const AllyTribute& tribute) {
  auto& marketplace = Game::Session::session_for(world).marketplace();
  if (tribute.request) {
    marketplace.queue_ally_request({.requester = owner_id,
                                    .giver = tribute.ally_owner,
                                    .resource = tribute.resource,
                                    .amount = tribute.amount});
    return;
  }
  const int sent = marketplace.send_to_ally(
      world, owner_id, tribute.ally_owner, tribute.resource, tribute.amount);
  marketplace.record_ally_answer({.requester = tribute.ally_owner,
                                  .giver = owner_id,
                                  .resource = tribute.resource,
                                  .requested = tribute.amount,
                                  .granted = sent,
                                  .verdict = Game::Systems::AllyTributeVerdict::Sent});
}
void apply_ally_call(World& world, int owner_id, const AllyCall& call) {
  auto& session = Game::Session::session_for(world);
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(call.target);
  const auto* transform = world.try_get<Engine::Core::TransformComponent>(call.target);
  if (unit == nullptr || transform == nullptr) {
    return;
  }
  auto& board = session.alliance();
  const auto call_id = board.next_call_id();
  for (const int ally : Game::Systems::ai_allies_of(session.owners(), owner_id)) {
    board.queue_call({.call_id = call_id,
                      .requester = owner_id,
                      .ally = ally,
                      .kind = call.kind,
                      .target = call.target,
                      .target_owner = unit->owner_id,
                      .target_x = transform->position.x,
                      .target_z = transform->position.z});
  }
}
void apply_ally_appeal_answer(World& world,
                              int owner_id,
                              const AllyAppealAnswer& answer) {
  auto& session = Game::Session::session_for(world);
  auto& board = session.alliance();
  const auto* appeal = board.open_appeal(answer.appeal_id);
  if (appeal == nullptr || appeal->to_owner != owner_id) {
    return;
  }
  int given = 0;
  if (answer.accept && appeal->kind == Game::Systems::AllyAppealKind::Resources) {
    given = session.marketplace().send_to_ally(
        world, owner_id, appeal->from_ally, appeal->resource, appeal->amount);
  }
  board.record_appeal_answer({.appeal_id = appeal->appeal_id,
                              .from_ally = appeal->from_ally,
                              .answerer = owner_id,
                              .accepted = answer.accept,
                              .given = given});
  board.close_appeal(answer.appeal_id);
}

} // namespace Game::Command::handlers
