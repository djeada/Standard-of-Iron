#include <gtest/gtest.h>

#include "app/orders/order_feedback.h"
#include "app/orders/order_markers.h"
#include "app/orders/order_submission.h"
#include "game/command/command.h"
#include "game/core/component.h"
#include "game/core/world.h"

namespace {

using App::Core::OrderKind;
using App::Core::OrderMarkerStore;
using App::Core::OrderOutcome;
using App::Core::OrderRequest;
using App::Core::OrderStatus;

class OrderFeedbackTest : public ::testing::Test {
protected:
  auto create_unit(float x,
                   float z,
                   int owner_id,
                   int health = 100) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    unit->owner_id = owner_id;
    unit->health = health;
    unit->max_health = 100;
    unit->spawn_type = Game::Units::SpawnType::Archer;
    return entity;
  }

  Engine::Core::World world;
};

TEST_F(OrderFeedbackTest, AcceptedAttackCarriesTheTargetAndUnitCount) {
  auto* archer = create_unit(0.0F, 0.0F, 1);
  auto* second = create_unit(1.0F, 0.0F, 1);
  auto* enemy = create_unit(5.0F, 0.0F, 2);

  OrderRequest request;
  request.kind = OrderKind::Attack;
  request.payload = Game::Command::AttackTarget{
      .units = {archer->get_id(), second->get_id()}, .target = enemy->get_id()};
  request.target = enemy->get_id();

  const auto outcome = App::Core::submit_player_order(world, 1, std::move(request));

  EXPECT_TRUE(outcome.accepted());
  EXPECT_EQ(outcome.kind, OrderKind::Attack);
  EXPECT_EQ(outcome.target, enemy->get_id());
  EXPECT_EQ(outcome.unit_count, 2U);
  EXPECT_TRUE(outcome.reason.isEmpty());
  EXPECT_EQ(outcome.rejection, Game::Command::Rejection::None);
  ASSERT_NE(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(OrderFeedbackTest, DeadTargetIsRejectedWithAPlayerReadableReason) {
  auto* archer = create_unit(0.0F, 0.0F, 1);
  auto* corpse = create_unit(5.0F, 0.0F, 2, 0);

  OrderRequest request;
  request.kind = OrderKind::Attack;
  request.payload = Game::Command::AttackTarget{.units = {archer->get_id()},
                                                .target = corpse->get_id()};
  request.target = corpse->get_id();

  const auto outcome = App::Core::submit_player_order(world, 1, std::move(request));

  EXPECT_TRUE(outcome.rejected());
  EXPECT_EQ(outcome.rejection, Game::Command::Rejection::DeadTarget);
  EXPECT_EQ(outcome.reason,
            App::Core::rejection_reason_text(Game::Command::Rejection::DeadTarget,
                                             OrderKind::Attack));
  EXPECT_EQ(outcome.target, corpse->get_id());
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(OrderFeedbackTest, FriendlyTargetIsRejectedAndNothingIsDispatched) {
  auto* archer = create_unit(0.0F, 0.0F, 1);
  auto* friendly = create_unit(5.0F, 0.0F, 1);

  OrderRequest request;
  request.kind = OrderKind::Attack;
  request.payload = Game::Command::AttackTarget{.units = {archer->get_id()},
                                                .target = friendly->get_id()};

  const auto outcome = App::Core::submit_player_order(world, 1, std::move(request));

  EXPECT_TRUE(outcome.rejected());
  EXPECT_EQ(outcome.rejection, Game::Command::Rejection::FriendlyTarget);
  EXPECT_FALSE(outcome.reason.isEmpty());
  EXPECT_EQ(archer->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
}

TEST_F(OrderFeedbackTest, EmptySubjectListIsRejectedAsNoSubjects) {
  auto* enemy = create_unit(5.0F, 0.0F, 2);

  OrderRequest request;
  request.kind = OrderKind::Attack;
  request.payload = Game::Command::AttackTarget{.units = {}, .target = enemy->get_id()};

  const auto outcome = App::Core::submit_player_order(world, 1, std::move(request));

  EXPECT_TRUE(outcome.rejected());
  EXPECT_EQ(outcome.rejection, Game::Command::Rejection::NoSubjects);
  EXPECT_EQ(outcome.reason, App::Core::no_eligible_units_reason(OrderKind::Attack));
}

TEST_F(OrderFeedbackTest, MoveKeepsTheDestinationOnTheOutcome) {
  auto* archer = create_unit(0.0F, 0.0F, 1);

  OrderRequest request;
  request.kind = OrderKind::Move;
  Game::Command::Move move;
  move.units = {archer->get_id()};
  move.targets = {QVector3D(3.0F, 0.0F, 4.0F)};
  request.payload = std::move(move);
  request.has_destination = true;
  request.destination = QVector3D(3.0F, 0.0F, 4.0F);

  const auto outcome = App::Core::submit_player_order(world, 1, std::move(request));

  EXPECT_TRUE(outcome.accepted());
  EXPECT_EQ(outcome.kind, OrderKind::Move);
  ASSERT_TRUE(outcome.has_destination);
  EXPECT_EQ(outcome.destination, QVector3D(3.0F, 0.0F, 4.0F));
  EXPECT_EQ(outcome.target, 0U);
}

TEST_F(OrderFeedbackTest, EveryRejectionHasAReasonAndEveryKindHasAName) {
  for (int i = 0; i <= static_cast<int>(Game::Command::Rejection::MalformedPayload);
       ++i) {
    const auto rejection = static_cast<Game::Command::Rejection>(i);
    if (rejection == Game::Command::Rejection::None) {
      EXPECT_TRUE(
          App::Core::rejection_reason_text(rejection, OrderKind::Move).isEmpty());
      continue;
    }
    EXPECT_FALSE(App::Core::rejection_reason_text(rejection, OrderKind::Move).isEmpty())
        << Game::Command::rejection_name(rejection);
  }
  for (int i = static_cast<int>(OrderKind::Move);
       i <= static_cast<int>(OrderKind::Formation);
       ++i) {
    const auto kind = static_cast<OrderKind>(i);
    EXPECT_STRNE(App::Core::order_kind_name(kind), "unknown");
    EXPECT_FALSE(App::Core::order_kind_display_name(kind).isEmpty());
  }
}

TEST_F(OrderFeedbackTest, AcceptedMessageNamesTheOrderAndTheGroupSize) {
  OrderOutcome single;
  single.kind = OrderKind::Attack;
  single.status = OrderStatus::Accepted;
  single.unit_count = 1;
  EXPECT_EQ(App::Core::accepted_order_message(single),
            App::Core::order_kind_display_name(OrderKind::Attack));

  OrderOutcome group = single;
  group.unit_count = 4;
  const QString message = App::Core::accepted_order_message(group);
  EXPECT_TRUE(message.contains(App::Core::order_kind_display_name(OrderKind::Attack)));
  EXPECT_TRUE(message.contains(QStringLiteral("4")));
}

TEST_F(OrderFeedbackTest, MarkerStoreAttachesToTheTargetAndFollowsIt) {
  auto* enemy = create_unit(5.0F, 0.0F, 2);
  OrderMarkerStore store;

  OrderOutcome outcome;
  outcome.kind = OrderKind::Attack;
  outcome.status = OrderStatus::Accepted;
  outcome.target = enemy->get_id();
  outcome.has_destination = true;
  outcome.destination = QVector3D(-50.0F, 0.0F, -50.0F);
  store.push(outcome, &world);

  ASSERT_EQ(store.markers().size(), 1U);
  EXPECT_EQ(store.markers().front().target, enemy->get_id());
  EXPECT_EQ(store.markers().front().position, QVector3D(5.0F, 0.0F, 0.0F));

  enemy->get_component<Engine::Core::TransformComponent>()->position.x = 7.0F;
  store.update(0.1F, &world);
  ASSERT_EQ(store.markers().size(), 1U);
  EXPECT_FLOAT_EQ(store.markers().front().position.x(), 7.0F);
  EXPECT_FALSE(store.markers().front().rejected);
}

TEST_F(OrderFeedbackTest, MarkerStoreUsesTheDestinationWhenThereIsNoTarget) {
  OrderMarkerStore store;
  OrderOutcome outcome;
  outcome.kind = OrderKind::Move;
  outcome.status = OrderStatus::Accepted;
  outcome.has_destination = true;
  outcome.destination = QVector3D(2.0F, 0.0F, 3.0F);
  store.push(outcome, &world);

  ASSERT_EQ(store.markers().size(), 1U);
  EXPECT_EQ(store.markers().front().target, 0U);
  EXPECT_EQ(store.markers().front().position, QVector3D(2.0F, 0.0F, 3.0F));
}

TEST_F(OrderFeedbackTest, MarkerStoreSkipsOutcomesWithoutAPlace) {
  OrderMarkerStore store;
  OrderOutcome outcome;
  outcome.kind = OrderKind::Hold;
  outcome.status = OrderStatus::Accepted;
  store.push(outcome, &world);
  EXPECT_TRUE(store.markers().empty());

  OrderOutcome not_issued;
  not_issued.kind = OrderKind::Move;
  not_issued.has_destination = true;
  store.push(not_issued, &world);
  EXPECT_TRUE(store.markers().empty());
}

TEST_F(OrderFeedbackTest, MarkersExpireAndDropWhenTheTargetDisappears) {
  auto* enemy = create_unit(5.0F, 0.0F, 2);
  OrderMarkerStore store;

  OrderOutcome attack;
  attack.kind = OrderKind::Attack;
  attack.status = OrderStatus::Accepted;
  attack.target = enemy->get_id();
  store.push(attack, &world);

  OrderOutcome refused;
  refused.kind = OrderKind::Move;
  refused.status = OrderStatus::Rejected;
  refused.has_destination = true;
  refused.destination = QVector3D(1.0F, 0.0F, 1.0F);
  store.push(refused, &world);
  ASSERT_EQ(store.markers().size(), 2U);
  EXPECT_TRUE(store.markers().back().rejected);

  world.destroy_entity(enemy->get_id());
  store.update(0.1F, &world);
  ASSERT_EQ(store.markers().size(), 1U);
  EXPECT_TRUE(store.markers().front().rejected);

  store.update(10.0F, &world);
  EXPECT_TRUE(store.markers().empty());
}

} // namespace
