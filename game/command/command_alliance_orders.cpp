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

void apply_ai_diplomacy(World& world, const Game::Systems::AllyTributeAnswer& answer) {
  auto& marketplace = Game::Session::session_for(world).marketplace();
  if (!marketplace.remove_ally_request(answer)) {
    return;
  }
  auto applied = answer;
  if (applied.granted > 0) {
    applied.granted = marketplace.send_to_ally(
        world, answer.giver, answer.requester, answer.resource, answer.granted);
  }
  marketplace.record_ally_answer(applied);
}

void apply_ai_diplomacy(World& world, int owner, const AIAllyCall& call) {
  auto& session = Game::Session::session_for(world);
  auto& board = session.alliance();
  const auto call_id = board.next_call_id();
  for (const int ally : Game::Systems::ai_allies_of(session.owners(), owner)) {
    board.queue_call({.call_id = call_id,
                      .requester = owner,
                      .ally = ally,
                      .kind = call.kind,
                      .target = call.target,
                      .target_owner = call.target_owner,
                      .target_x = call.target_x,
                      .target_z = call.target_z});
  }
}

void apply_ai_diplomacy(World& world, const Game::Systems::AllyCallAnswer& answer) {
  auto& board = Game::Session::session_for(world).alliance();
  if (board.remove_call(answer.call_id, answer.ally)) {
    board.record_call_answer(answer);
  }
}

void apply_ai_diplomacy(World& world, const Game::Systems::AllyAppeal& appeal) {
  Game::Session::session_for(world).alliance().record_appeal(appeal);
}

void apply_ai_diplomacy(World& world, const AIAllyAppealReply& input) {
  auto& board = Game::Session::session_for(world).alliance();
  const auto& reply = input.reply;
  if (input.answers_player &&
      !board.remove_appeal_answer(reply.appeal_id, reply.from_ally)) {
    return;
  }
  if (reply.follow_up == Game::Systems::AllyAppealFollowUp::Withdrawn) {
    board.close_appeal(reply.appeal_id);
  }
  if (input.goodwill_change != 0.0F) {
    board.change_goodwill(reply.from_ally, reply.to_owner, input.goodwill_change);
  }
  board.record_appeal_reply(reply);
}

void apply_garrison_leash(World& world, int owner, const SetGarrisonLeash& leash) {
  for (auto [id, unit, controlled] :
       world.view<const Engine::Core::UnitComponent,
                  Engine::Core::AIControlledComponent>()) {
    if (unit.owner_id != owner || unit.health <= 0 ||
        !Game::Units::is_troop_spawn(unit.spawn_type)) {
      continue;
    }
    if (controlled.sallying) {
      const auto* attack = world.try_get<Engine::Core::AttackTargetComponent>(id);
      const auto* target =
          attack != nullptr
              ? world.try_get<Engine::Core::UnitComponent>(attack->target_id)
              : nullptr;
      controlled.sallying = target != nullptr && target->health > 0 &&
                            Game::Units::is_siege_engine_spawn(target->spawn_type);
    }
    const auto* wave = world.try_get<Engine::Core::AssaultWaveComponent>(id);
    const bool leashed = !controlled.sallying && (wave == nullptr || !wave->active);
    controlled.leash_x = leash.x;
    controlled.leash_z = leash.z;
    controlled.leash_half_x = leashed ? leash.half_x : 0.0F;
    controlled.leash_half_z = leashed ? leash.half_z : 0.0F;
  }
}

} // namespace Game::Command::handlers
