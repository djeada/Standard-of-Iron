#include <gtest/gtest.h>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "game/command/command_queue.h"
#include "game/command/command_validator.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"

namespace {

using Engine::Core::EntityID;
using Game::Command::Command;
using Game::Command::CommandQueue;
using Game::Command::Rejection;
using Game::Command::Source;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

struct Match {
  Match() {
    scope = std::make_unique<ScopedSession>(session);
    auto& owners = session.owners();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "player");
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "enemy");
    owners.set_owner_team(1, 1);
    owners.set_owner_team(2, 2);
  }

  auto spawn(int owner_id, float x, float z) -> EntityID {
    auto* entity = session.world().create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position.x = x;
    transform->position.z = z;
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
    unit->owner_id = owner_id;
    return entity->get_id();
  }

  SessionContext session;
  std::unique_ptr<ScopedSession> scope;
};

TEST(CommandValidatorTest, RejectsOrdersAddressedToSomeoneElsesUnits) {
  Match match;
  const EntityID enemy = match.spawn(2, 5.0F, 5.0F);

  const Command command{
      .source = Source::LocalPlayer,
      .owner_id = 1,
      .payload = Game::Command::Move{.units = {enemy},
                                     .targets = {QVector3D(1.0F, 0.0F, 1.0F)}}};

  const auto validation = Game::Command::validate(match.session.world(), command);
  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::NoSubjects);
}

TEST(CommandValidatorTest, JudgesPlayerAndAiOrdersIdentically) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID theirs = match.spawn(2, 5.0F, 5.0F);

  const Game::Command::Payload payload = Game::Command::Move{
      .units = {mine, theirs},
      .targets = {QVector3D(1.0F, 0.0F, 1.0F), QVector3D(2.0F, 0.0F, 2.0F)}};

  const auto as_player = Game::Command::validate(
      match.session.world(),
      Command{.source = Source::LocalPlayer, .owner_id = 1, .payload = payload});
  const auto as_ai = Game::Command::validate(
      match.session.world(),
      Command{.source = Source::AI, .owner_id = 1, .payload = payload});

  ASSERT_TRUE(as_player.accepted());
  ASSERT_TRUE(as_ai.accepted());

  const auto& player_move = std::get<Game::Command::Move>(as_player.command.payload);
  const auto& ai_move = std::get<Game::Command::Move>(as_ai.command.payload);
  EXPECT_EQ(player_move.units, ai_move.units);
  EXPECT_EQ(player_move.units, std::vector<EntityID>{mine});
}

TEST(CommandValidatorTest, KeepsUnitsAndTargetsAlignedWhenFiltering) {
  Match match;
  const EntityID first = match.spawn(1, 0.0F, 0.0F);
  const EntityID stranger = match.spawn(2, 1.0F, 1.0F);
  const EntityID second = match.spawn(1, 2.0F, 2.0F);

  const Command command{
      .source = Source::LocalPlayer,
      .owner_id = 1,
      .payload = Game::Command::Move{.units = {first, stranger, second},
                                     .targets = {QVector3D(10.0F, 0.0F, 0.0F),
                                                 QVector3D(20.0F, 0.0F, 0.0F),
                                                 QVector3D(30.0F, 0.0F, 0.0F)}}};

  const auto validation = Game::Command::validate(match.session.world(), command);
  ASSERT_TRUE(validation.accepted());

  const auto& move = std::get<Game::Command::Move>(validation.command.payload);
  ASSERT_EQ(move.units.size(), 2U);
  ASSERT_EQ(move.targets.size(), 2U);
  EXPECT_EQ(move.units[0], first);
  EXPECT_FLOAT_EQ(move.targets[0].x(), 10.0F);
  EXPECT_EQ(move.units[1], second);

  EXPECT_FLOAT_EQ(move.targets[1].x(), 30.0F);
}

TEST(CommandValidatorTest, RefusesToAttackAnAlly) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID friendly = match.spawn(1, 1.0F, 1.0F);

  const auto validation =
      Game::Command::validate(match.session.world(),
                              Command{.source = Source::LocalPlayer,
                                      .owner_id = 1,
                                      .payload = Game::Command::AttackTarget{
                                          .units = {mine}, .target = friendly}});

  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::FriendlyTarget);
}

TEST(CommandValidatorTest, RefusesToAttackAHandleThatNoLongerResolves) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID enemy = match.spawn(2, 5.0F, 5.0F);
  match.session.world().destroy_entity(enemy);

  const auto validation =
      Game::Command::validate(match.session.world(),
                              Command{.source = Source::LocalPlayer,
                                      .owner_id = 1,
                                      .payload = Game::Command::AttackTarget{
                                          .units = {mine}, .target = enemy}});

  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::DeadTarget);
}

TEST(CommandValidatorTest, RejectsAnOrderWithNoIssuer) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  const auto validation =
      Game::Command::validate(match.session.world(),
                              Command{.source = Source::Script,
                                      .owner_id = 0,
                                      .payload = Game::Command::Stop{.units = {mine}}});

  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::NoOwner);
}

TEST(CommandQueueTest, AppliesNothingUntilDrained) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  auto& queue = match.session.commands();
  queue.submit(
      Source::LocalPlayer,
      1,
      Game::Command::Move{.units = {mine}, .targets = {QVector3D(9.0F, 0.0F, 9.0F)}});

  EXPECT_EQ(queue.pending(), 1U);
  auto* entity = match.session.world().get_entity(mine);
  ASSERT_NE(entity, nullptr);
  EXPECT_EQ(entity->get_component<Engine::Core::MovementComponent>(), nullptr);

  EXPECT_EQ(queue.drain(match.session.world(), 1), 1U);
  EXPECT_EQ(queue.pending(), 0U);
  EXPECT_NE(entity->get_component<Engine::Core::MovementComponent>(), nullptr);
}

TEST(CommandQueueTest, PreservesSubmissionOrderAcrossSources) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  std::vector<Source> executed;
  auto& queue = match.session.commands();
  queue.set_observer(
      [&executed](const Command& command) { executed.push_back(command.source); });

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});
  queue.submit(Source::AI, 1, Game::Command::Stop{.units = {mine}});
  queue.submit(Source::Replay, 1, Game::Command::Stop{.units = {mine}});
  queue.drain(match.session.world(), 7);

  ASSERT_EQ(executed.size(), 3U);
  EXPECT_EQ(executed[0], Source::LocalPlayer);
  EXPECT_EQ(executed[1], Source::AI);
  EXPECT_EQ(executed[2], Source::Replay);
}

TEST(CommandQueueTest, ObserverSeesEveryAcceptedCommandForReplay) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID theirs = match.spawn(2, 5.0F, 5.0F);

  std::vector<Command> recorded;
  auto& queue = match.session.commands();
  queue.set_observer(
      [&recorded](const Command& command) { recorded.push_back(command); });

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {theirs}});
  queue.drain(match.session.world(), 42);

  ASSERT_EQ(recorded.size(), 1U);
  EXPECT_EQ(recorded.front().submitted_tick, 42U);
  EXPECT_EQ(queue.accepted_count(), 1U);
  EXPECT_EQ(queue.rejected_count(), 1U);
}

TEST(CommandQueueTest, ReportsWhyACommandWasThrownOut) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID friendly = match.spawn(1, 1.0F, 1.0F);

  std::vector<Rejection> rejections;
  auto& queue = match.session.commands();
  queue.set_rejection_observer([&rejections](const Command&, Rejection reason) {
    rejections.push_back(reason);
  });

  queue.submit(
      Source::AI, 1, Game::Command::AttackTarget{.units = {mine}, .target = friendly});
  queue.drain(match.session.world(), 1);

  ASSERT_EQ(rejections.size(), 1U);
  EXPECT_EQ(rejections.front(), Rejection::FriendlyTarget);
}

TEST(CommandQueueTest, SubmittingDuringADrainDefersToTheNextTick) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  auto& queue = match.session.commands();
  bool resubmitted = false;
  queue.set_observer([&](const Command&) {
    if (!resubmitted) {
      resubmitted = true;
      queue.submit(Source::Script, 1, Game::Command::Stop{.units = {mine}});
    }
  });

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});
  EXPECT_EQ(queue.drain(match.session.world(), 1), 1U);

  EXPECT_EQ(queue.pending(), 1U);
  EXPECT_EQ(queue.drain(match.session.world(), 2), 1U);
}

TEST(CommandSubmitTest, RoutesThroughTheSessionQueueWhenThereIsOne) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  Game::Command::submit(match.session.world(),
                        Source::LocalPlayer,
                        1,
                        Game::Command::Stop{.units = {mine}});

  EXPECT_EQ(match.session.commands().pending(), 1U);
}

TEST(CommandSubmitTest, AppliesImmediatelyForAWorldWithNoSession) {

  Engine::Core::World world;
  auto* entity = world.create_entity();
  entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
  unit->owner_id = 1;

  Game::Command::submit(world,
                        Source::Script,
                        1,
                        Game::Command::Move{.units = {entity->get_id()},
                                            .targets = {QVector3D(4.0F, 0.0F, 4.0F)}});

  EXPECT_NE(entity->get_component<Engine::Core::MovementComponent>(), nullptr);
}

} // namespace
